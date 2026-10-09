#include "CommandContext.h"
#include <Graphics/Pipeline/GraphicsPipelineState.h>
#include <Graphics/Buffers/VertexBuffer.h>
#include <Graphics/Buffers/IndexBuffer.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthBuffer.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <Graphics/Commands/IndexedDrawPacket.h>
#include <Graphics/Buffers/ConstantBufferArena.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	CommandContext::CommandContext(GraphicsDevice& device)
	{
		HRESULT result = S_FALSE;
		auto* d3dDevice = device.GetDevice();

		// CommandAllocatorの作成
		result = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("CommandAllocatorの作成に失敗");
		}
		// CommandListの作成
		result = d3dDevice->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("CommandListの作成に失敗");
		}

		// Listを閉じる
		result = commandList_->Close();
		if (FAILED(result))
		{
			throw std::runtime_error("CommandListのCloseに失敗");
		}
	}

	void CommandContext::Begin()
	{
		// Frame所有のContextはFrameResourcesからだけ開始する
		if (frameOwned_ && !frameBegin_)
		{
			throw std::logic_error("Frame-owned Context Begin must go through FrameResources.");
		}
		frameBegin_ = false;
		if (recording_)
		{
			throw std::logic_error("CommandContextは既に記録中です。");
		}
		if (failed_)
		{
			throw std::logic_error("CommandContextは使用禁止状態です。");
		}

		// CommandAllocatorをリセット
		if (FAILED(commandAllocator_->Reset()))
		{
			Invalidate();
			throw std::runtime_error("CommandAllocatorのResetに失敗");
		}

		// CommandListをリセット
		if (FAILED(commandList_->Reset(commandAllocator_.Get(), nullptr)))
		{
			Invalidate();
			throw std::runtime_error("CommandListのResetに失敗");
		}

		recording_ = true;
	}

	void CommandContext::End()
	{
		if (!recording_)
		{
			throw std::logic_error("CommandContextは記録中ではありません。");
		}
		if (failed_)
		{
			throw std::logic_error("CommandContextは使用禁止状態です。");
		}

		// CommandListを閉じる
		if (FAILED(commandList_->Close()))
		{
			Invalidate();
			throw std::runtime_error("CommandListのCloseに失敗");
		}

		recording_ = false;
	}

	ID3D12GraphicsCommandList* CommandContext::GetRecordingList() const
	{
		if (!recording_)
		{
			throw std::logic_error("CommandContextは記録中ではありません。");
		}
		if (failed_)
		{
			throw std::logic_error("CommandContextは使用禁止状態です。");
		}
		return commandList_.Get();
	}

	ID3D12CommandList* CommandContext::GetExecutableList() const
	{
		// Frame所有のContextはFrameResourcesからだけ送信する
		if (frameOwned_ && !frameSubmit_)
		{
			throw std::logic_error("Frame-owned Context submission must go through FrameResources.");
		}
		if (recording_)
		{
			throw std::logic_error("CommandContextは記録中です。");
		}
		if (failed_)
		{
			throw std::logic_error("CommandContextは使用禁止状態です。");
		}
		return commandList_.Get();
	}

	void CommandContext::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
	{
		if (!resource)
		{
			throw std::invalid_argument("resourceがnullptrです。");
		}
		// 記録中のListを取得する。同一状態でもContextの使用可否を確認する。
		auto* commandList = GetRecordingList();

		if (before == after)
		{
			return;
		}

		// リソースバリアを作成
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// リソースバリアを設定
		commandList->ResourceBarrier(1, &barrier);
	}

	void CommandContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const std::array<float, 4>& color)
	{
		if (rtv.ptr == 0)
		{
			throw std::invalid_argument("rtvが無効です。");
		}

		// 記録中のListを取得する
		auto* commandList = GetRecordingList();

		// レンダーターゲットをクリア
		commandList->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
	}

	void CommandContext::DrawIndexed(const GraphicsPipelineState& pipeline, const VertexBuffer& vertices, const IndexBuffer& indices,
		const ColorTargetView& target, const DepthBuffer& depth, const ConstantBufferArena& constants,
		std::span<const RootConstantBinding> bindings)
	{
		try
		{
			// 記録中のListを取得する
			auto* commandList = GetRecordingList();

			// このContextと同じFrameの定数か確認する
			if (frameConstants_ != &constants)
			{
				throw std::invalid_argument("Draw constants must belong to this Context's FrameResources.");
			}

			// 頂点・index・描画先・定数の組合せを確認する
			if (vertices.GetView().StrideInBytes != pipeline.GetVertexStride() || indices.GetMaximumIndex() >= vertices.GetCount() ||
				indices.GetCount() % 3 != 0 || target.GetFormat() != pipeline.GetTargetFormat() ||
				depth.GetTexture().GetFormat() != pipeline.GetDepthFormat() || target.GetWidth() != depth.GetTexture().GetWidth() ||
				target.GetHeight() != depth.GetTexture().GetHeight() || bindings.size() != pipeline.GetRootParameterCount() ||
				bindings.size() > 32)
			{
				throw std::invalid_argument("Indexed draw layout/index/target/constants mismatch.");
			}

			// 描画に使う部品が、記録先と同じDeviceに属するか確認する
			ComPtr<ID3D12Device> device;
			CheckGraphicsResult(commandList->GetDevice(IID_PPV_ARGS(device.GetAddressOf())), "Recording device query failed.");
			for (auto* object : std::array<ID3D12DeviceChild*, 6>{pipeline.GetState(), vertices.GetResource(), indices.GetResource(),
					 target.GetResource(), depth.GetTexture().GetResource(), constants.GetResource()})
			{
				RequireSameDevice(object, device.Get());
			}

			// root parameterの範囲・重複と定数sliceを確認する
			std::array<D3D12_GPU_VIRTUAL_ADDRESS, 32> addresses{};
			for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
			{
				if (bindings[bindingIndex].rootParameter >= pipeline.GetRootParameterCount())
				{
					throw std::invalid_argument("Root parameter is out of range.");
				}
				for (std::size_t previousBindingIndex = 0; previousBindingIndex < bindingIndex; ++previousBindingIndex)
				{
					if (bindings[previousBindingIndex].rootParameter == bindings[bindingIndex].rootParameter)
					{
						throw std::invalid_argument("Duplicate root parameter.");
					}
				}
				addresses[bindingIndex] = constants.GetAddress(bindings[bindingIndex].slice);
			}

			// 全検査後に、描画範囲と描画先を設定する
			const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(target.GetWidth()), static_cast<float>(target.GetHeight()), 0, 1};
			const D3D12_RECT scissor{0, 0, static_cast<LONG>(target.GetWidth()), static_cast<LONG>(target.GetHeight())};
			const auto rtv = target.GetRtv();
			const auto dsv = depth.GetDsv();
			commandList->SetGraphicsRootSignature(pipeline.GetRootSignature());
			commandList->SetPipelineState(pipeline.GetState());
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissor);
			commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

			// 頂点・index・定数を設定する
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			const auto& vertexView = vertices.GetView();
			const auto& indexView = indices.GetView();
			commandList->IASetVertexBuffers(0, 1, &vertexView);
			commandList->IASetIndexBuffer(&indexView);
			for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
			{
				commandList->SetGraphicsRootConstantBufferView(bindings[bindingIndex].rootParameter, addresses[bindingIndex]);
			}

			// 全indexの描画を記録する
			commandList->DrawIndexedInstanced(indices.GetCount(), 1, 0, 0, 0);
		}
		catch (...)
		{
			// 失敗したContextの記録・送信を禁止する
			Invalidate();
			throw;
		}
	}

	void CommandContext::ClearDepth(const DepthBuffer& depth)
	{
		try
		{
			// 記録中のListと記録先Deviceを取得する
			auto* commandList = GetRecordingList();
			ComPtr<ID3D12Device> device;
			CheckGraphicsResult(commandList->GetDevice(IID_PPV_ARGS(device.GetAddressOf())), "Recording device query failed.");

			// 深度画像が記録先と同じDeviceに属するか確認する
			RequireSameDevice(depth.GetTexture().GetResource(), device.Get());

			// Reverse-Zの全域clear0を記録する
			commandList->ClearDepthStencilView(depth.GetDsv(), D3D12_CLEAR_FLAG_DEPTH, 0, 0, 0, nullptr);
		}
		catch (...)
		{
			Invalidate();
			throw;
		}
	}

	// 記録失敗時にContextを使用禁止にする。GPU待機や命令の取り消しは行わない
	void CommandContext::Invalidate() noexcept
	{
		failed_ = true;
	}

	// packet・描画先・同じFrameの定数を検査し、全indexの描画を記録する
	void CommandContext::DrawIndexed(
		const IndexedDrawPacket& packet, const ColorTargetView& color, const DepthTargetView& depth, const ConstantBufferArena& constants)
	{
		try
		{
			// 記録中のListを取得する
			auto* commandList = GetRecordingList();

			// packetの参照先を確認する
			if (!packet.pipeline || !packet.vertices || !packet.indices)
			{
				throw std::invalid_argument("IndexedDrawPacketのpipeline・vertices・indicesがnullptrです");
			}
			const auto& pipeline = *packet.pipeline;
			const auto& vertices = *packet.vertices;
			const auto& indices = *packet.indices;
			const auto bindings = packet.bindings;

			// このContextと同じFrameの定数か確認する
			if (frameConstants_ != &constants)
			{
				throw std::invalid_argument("定数がこのContextと同じFrameResourcesに属していません");
			}

			// 期待する描画サイズと、色・深度両方のサイズを確認する
			if (packet.expectedWidth == 0 || packet.expectedHeight == 0)
			{
				throw std::invalid_argument("IndexedDrawPacketのexpectedWidthまたはexpectedHeightが0です");
			}
			if (packet.expectedWidth != color.GetWidth() || packet.expectedHeight != color.GetHeight() ||
				packet.expectedWidth != depth.GetWidth() || packet.expectedHeight != depth.GetHeight())
			{
				throw std::invalid_argument("IndexedDrawPacketの描画サイズが色または深度のサイズと一致しません");
			}
			if (color.GetRtv().ptr == 0 || depth.GetDsv().ptr == 0)
			{
				throw std::invalid_argument("描画先のRTVまたはDSVが無効です");
			}

			// 頂点・index・描画先・定数の組合せを確認する
			if (vertices.GetView().StrideInBytes != pipeline.GetVertexStride() || indices.GetMaximumIndex() >= vertices.GetCount() ||
				indices.GetCount() % 3 != 0 || color.GetFormat() != pipeline.GetTargetFormat() ||
				depth.GetFormat() != pipeline.GetDepthFormat() || bindings.size() != pipeline.GetRootParameterCount() ||
				bindings.size() > 32)
			{
				throw std::invalid_argument("頂点・index・描画先・定数の組合せが一致しません");
			}

			// 描画に使う部品が、記録先と同じDeviceに属するか確認する
			ComPtr<ID3D12Device> device;
			CheckGraphicsResult(commandList->GetDevice(IID_PPV_ARGS(device.GetAddressOf())), "記録先Deviceの取得に失敗");
			for (auto* object : std::array<ID3D12DeviceChild*, 6>{pipeline.GetState(), vertices.GetResource(), indices.GetResource(),
					 color.GetResource(), depth.GetResource(), constants.GetResource()})
			{
				RequireSameDevice(object, device.Get());
			}

			// root parameterの範囲・重複と定数sliceを確認する
			std::array<D3D12_GPU_VIRTUAL_ADDRESS, 32> addresses{};
			for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
			{
				if (bindings[bindingIndex].rootParameter >= pipeline.GetRootParameterCount())
				{
					throw std::invalid_argument("root parameterが範囲外です");
				}
				for (std::size_t previousBindingIndex = 0; previousBindingIndex < bindingIndex; ++previousBindingIndex)
				{
					if (bindings[previousBindingIndex].rootParameter == bindings[bindingIndex].rootParameter)
					{
						throw std::invalid_argument("root parameterが重複しています");
					}
				}
				addresses[bindingIndex] = constants.GetAddress(bindings[bindingIndex].slice);
			}

			// 全検査後に、描画範囲と描画先を設定する
			const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(color.GetWidth()), static_cast<float>(color.GetHeight()), 0, 1};
			const D3D12_RECT scissor{0, 0, static_cast<LONG>(color.GetWidth()), static_cast<LONG>(color.GetHeight())};
			const auto rtv = color.GetRtv();
			const auto dsv = depth.GetDsv();
			commandList->SetGraphicsRootSignature(pipeline.GetRootSignature());
			commandList->SetPipelineState(pipeline.GetState());
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissor);
			commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

			// 頂点・index・定数を設定する
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			const auto& vertexView = vertices.GetView();
			const auto& indexView = indices.GetView();
			commandList->IASetVertexBuffers(0, 1, &vertexView);
			commandList->IASetIndexBuffer(&indexView);
			for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
			{
				commandList->SetGraphicsRootConstantBufferView(bindings[bindingIndex].rootParameter, addresses[bindingIndex]);
			}

			// 全indexの描画を記録する
			commandList->DrawIndexedInstanced(indices.GetCount(), 1, 0, 0, 0);
		}
		catch (...)
		{
			// 失敗したContextの記録・送信を禁止する
			Invalidate();
			throw;
		}
	}

	// 借用DSVにD32/Reverse-Zの全域clear0を記録する。
	void CommandContext::ClearDepth(const DepthTargetView& depth)
	{
		try
		{
			// 記録中のListと借用DSVを取得する
			auto* commandList = GetRecordingList();
			const auto dsv = depth.GetDsv();

			// Reverse-Zの全域clear0を記録する
			commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
		}
		catch (...)
		{
			Invalidate();
			throw;
		}
	}

}
