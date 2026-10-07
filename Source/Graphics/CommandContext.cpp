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
}