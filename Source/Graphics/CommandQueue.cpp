#include <Graphics/CommandQueue.h>
#include <Core/Log.h>
#include <stdexcept>
#include <limits>

namespace KT::Graphics
{
	CommandQueue::CommandQueue(GraphicsDevice& device)
	{
		HRESULT result = S_FALSE;
		auto* d3dDevice = device.GetDevice();

		// コマンドキューの作成
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		result = d3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(queue_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("CommandQueueの作成に失敗");
		}

		// Fenceの作成
		result = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("Fenceの作成に失敗");
		}
	}

	std::uint64_t CommandQueue::Signal()
	{
		// 最大値チェック
		if (nextFenceValue_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Fenceの値が最大値に達しました。");
		}

		// 今回送るFenceの目印の値を取得
		auto value = nextFenceValue_;

		// FenceにSignalを送る
		HRESULT result = queue_->Signal(fence_.Get(), value);
		if (FAILED(result))
		{
			throw std::runtime_error("FenceのSignalに失敗");
		}

		// 次のFenceの値を進める
		++nextFenceValue_;
		
		return value;
	}

	bool CommandQueue::IsComplete(std::uint64_t fenceValue) const
	{
		// fenceValueが次のFenceの値以上なら不正
		if (fenceValue >= nextFenceValue_)
		{
			throw std::invalid_argument("fenceValueが次のFenceの値以上です。");
		}

		// Deviceが失われていないか確認
		auto completedValue = fence_->GetCompletedValue();
		if (completedValue == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Deviceが失われたため、Fenceの完了を確認できません。");
		}

		return completedValue >= fenceValue;
	}

	void CommandQueue::Wait(std::uint64_t fenceValue) const
	{
		// 指定した目印までGPUが完了しているか確認
		if (IsComplete(fenceValue))
		{
			return;
		}

		// Fenceの完了を待つ
		HRESULT result = fence_->SetEventOnCompletion(fenceValue, nullptr);
		if (FAILED(result))
		{
			throw std::runtime_error("FenceのSetEventOnCompletionに失敗");
		}

		// 再度、Fenceの完了を確認
		if (!IsComplete(fenceValue))
		{
			throw std::runtime_error("Fenceの完了を待つことができませんでした。");
		}
	}
}