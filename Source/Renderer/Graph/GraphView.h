#pragma once
#include <Renderer/Graph/GraphResourceHandle.h>
#include <Renderer/Graph/GraphTextureRange.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <string>
#include <Graphics/Textures/PixelDepthView.h>
#include <variant>

namespace KT::Renderer
{
	// monostateは未設定で必ず拒否。RTV/DSVの排他種別はvariantから得る。
	// 値copyしてもGPU画像/descriptor所有権は持たない。元wrapperの寿命は不要。
	// PixelDepthViewは読取専用深度SRV。UAV/任意Color SRVは扱わない。
	using GraphViewBinding =
		std::variant<std::monostate, KT::Graphics::ColorTargetView, KT::Graphics::DepthTargetView, KT::Graphics::PixelDepthView>;

	struct GraphViewDesc
	{
		std::string name;
		GraphResourceHandle resource{};
		GraphTextureRange range{};
		GraphViewBinding binding{};
	};
}
