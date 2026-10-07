#include "CommandContext.h"
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
		if (recording_)
		{
			throw std::logic_error("CommandContextは既に記録中です。");
		}

		// CommandAllocatorをリセット
		if (FAILED(commandAllocator_->Reset()))
		{
			throw std::runtime_error("CommandAllocatorのResetに失敗");
		}

		// CommandListをリセット
		if (FAILED(commandList_->Reset(commandAllocator_.Get(), nullptr)))
		{
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

		// CommandListを閉じる
		if (FAILED(commandList_->Close()))
		{
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
		return commandList_.Get();
	}

	ID3D12CommandList* CommandContext::GetExecutableList() const
	{
		if (recording_)
		{
			throw std::logic_error("CommandContextは記録中です。");
		}
		return commandList_.Get();
	}
}