#pragma once
#include <d3d12.h>
#include <string>

namespace KT::Renderer
{
	// 外部画像だけを借用。RTV/DSV/kind/targetViewはここへ置かない。
	// Graph破棄では画像を解放しない。所有者は最後の利用Fence完了まで保持する。
	struct GraphImportedTextureDesc
	{
		std::string name;
		ID3D12Resource* resource = nullptr;
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;
		// 初期内容の外部保証。現在は画像全体の真偽、stateや完了とは別。
		bool contentsDefined = false;
		bool requireDefineAtEnd = false;
	};
}
