#include "CommandContext.h"
#include <Graphics/GraphicsPipelineState.h>
#include <Graphics/VertexBuffer.h>
#include <Graphics/IndexBuffer.h>
#include <Graphics/ColorTargetView.h>
#include <Graphics/DepthBuffer.h>
#include <Graphics/ConstantBufferArena.h>
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
		result = d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.GetAddressOf()));
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
		if (frameOwned_ && !frameBegin_) throw std::logic_error("Frame-owned Context Begin must go through FrameResources.");
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
		if (frameOwned_ && !frameSubmit_) throw std::logic_error("Frame-owned Context submission must go through FrameResources.");
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
		auto* list = GetRecordingList();

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
		list->ResourceBarrier(1, &barrier);
	}

	void CommandContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const std::array<float, 4>& color)
	{
		if (rtv.ptr == 0)
		{
			throw std::invalid_argument("rtvが無効です。");
		}

		// 記録中のListを取得する
		auto* list = GetRecordingList();

		// レンダーターゲットをクリア
		list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
	}

	void CommandContext::DrawIndexed(const GraphicsPipelineState& p, const VertexBuffer& vertices, const IndexBuffer& indices,
		const ColorTargetView& target, const DepthBuffer& depth, const ConstantBufferArena& constants,
		std::span<const RootConstantBinding> bindings)
	{
		try
		{
			auto* list=GetRecordingList();
			if (frameConstants_!=&constants) throw std::invalid_argument("Draw constants must belong to this Context's FrameResources.");
			if (vertices.GetView().StrideInBytes!=p.GetVertexStride() || indices.GetMaximumIndex()>=vertices.GetCount() ||
				indices.GetCount()%3!=0 || target.GetFormat()!=p.GetTargetFormat() || depth.GetTexture().GetFormat()!=p.GetDepthFormat() ||
				target.GetWidth()!=depth.GetTexture().GetWidth() || target.GetHeight()!=depth.GetTexture().GetHeight() ||
				bindings.size()!=p.GetRootParameterCount() || bindings.size()>32)
				throw std::invalid_argument("Indexed draw layout/index/target/constants mismatch.");
			ComPtr<ID3D12Device> device;
			CheckGraphicsResult(list->GetDevice(IID_PPV_ARGS(device.GetAddressOf())),"Recording device query failed.");
			for (auto* object:std::array<ID3D12DeviceChild*,6>{p.GetState(),vertices.GetResource(),indices.GetResource(),
				target.GetResource(),depth.GetTexture().GetResource(),constants.GetResource()}) RequireSameDevice(object,device.Get());
			std::array<D3D12_GPU_VIRTUAL_ADDRESS,32> addresses{};
			for (std::size_t i=0; i<bindings.size(); ++i)
			{
				if (bindings[i].rootParameter>=p.GetRootParameterCount()) throw std::invalid_argument("Root parameter is out of range.");
				for (std::size_t j=0; j<i; ++j)
					if (bindings[j].rootParameter==bindings[i].rootParameter) throw std::invalid_argument("Duplicate root parameter.");
				addresses[i]=constants.GetAddress(bindings[i].slice);
			}
			// 全検査完了後だけ記録。GraphicsはView/Object/Materialの意味を知らない。
			const D3D12_VIEWPORT viewport{0,0,float(target.GetWidth()),float(target.GetHeight()),0,1};
			const D3D12_RECT scissor{0,0,LONG(target.GetWidth()),LONG(target.GetHeight())};
			const auto rtv=target.GetRtv(), dsv=depth.GetDsv();
			list->SetGraphicsRootSignature(p.GetRootSignature()); list->SetPipelineState(p.GetState());
			list->RSSetViewports(1,&viewport); list->RSSetScissorRects(1,&scissor);
			list->OMSetRenderTargets(1,&rtv,FALSE,&dsv);
			list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			const auto& vb=vertices.GetView(); const auto& ib=indices.GetView();
			list->IASetVertexBuffers(0,1,&vb); list->IASetIndexBuffer(&ib);
			for (std::size_t i=0; i<bindings.size(); ++i) list->SetGraphicsRootConstantBufferView(bindings[i].rootParameter,addresses[i]);
			list->DrawIndexedInstanced(indices.GetCount(),1,0,0,0);
		}
		catch (...) { Invalidate(); throw; }
	}
	void CommandContext::ClearDepth(const DepthBuffer& depth)
	{
		try
		{
			auto* list=GetRecordingList();
			ComPtr<ID3D12Device> device;
			CheckGraphicsResult(list->GetDevice(IID_PPV_ARGS(device.GetAddressOf())),"Recording device query failed.");
			RequireSameDevice(depth.GetTexture().GetResource(),device.Get());
			list->ClearDepthStencilView(depth.GetDsv(),D3D12_CLEAR_FLAG_DEPTH,0,0,0,nullptr);
		}
		catch (...) { Invalidate(); throw; }
	}

	// 記録失敗時にContextを使用禁止にする。GPU待機や命令の取り消しは行わない
	void CommandContext::Invalidate() noexcept
	{
		failed_ = true;
	}
}
