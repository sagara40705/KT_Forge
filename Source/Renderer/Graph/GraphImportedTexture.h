#pragma once
#include <string>
#include <d3d12.h>

namespace KT::Renderer
{
	// グラフにインポートされたテクスチャの情報
	struct GraphImportedTextureDesc
	{
		// テクスチャの名前
		std::string name;

		// 使う画像
		ID3D12Resource* resource = nullptr;

		// 描画先View
		D3D12_CPU_DESCRIPTOR_HANDLE rtv{};

		// Graphの処理開始時に画像が置かれている状態
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		// Graphの処理終了時に戻しておく状態
		D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;

		// 最初から読み取り可能な内容があるか
		bool contentsDefined = false;
	};
}