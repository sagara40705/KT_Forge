#pragma once
#include <Content/TextureAsset.h>
#include <filesystem>

namespace KT::Content
{
	// DirectXTexの画像形式をCPU共通のRGBA8へ変換する。GPU操作は行わない。
	class TextureLoader
	{
	public:
		// 解決済みパスを受ける。プロジェクト外参照の検査はProjectAssetsが担当する。
		[[nodiscard]] static TextureAsset Load(const TextureAsset& definition, const std::filesystem::path& path);
	};
}
