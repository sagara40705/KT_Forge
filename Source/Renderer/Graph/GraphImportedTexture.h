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

		// 外部所有の画像を借用する。所有者はGPUの利用完了まで保持する
		ID3D12Resource* resource = nullptr;

		// 描画先RTVのCPUハンドル。Graphはdescriptor heapを所有しない
		D3D12_CPU_DESCRIPTOR_HANDLE rtv{};

		// Graphの処理開始時に画像が置かれている状態
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		// Graphの処理終了時に要求する状態。保存しただけでは遷移しない
		D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_COMMON;

		// 最初から読み取り可能な内容があるか
		bool contentsDefined = false;

		// Graph終了時に画像全体の内容が定義済みであることを要求する
		bool requireDefineAtEnd = false;
	};
}