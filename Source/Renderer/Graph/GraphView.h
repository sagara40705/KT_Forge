#pragma once
#include <Renderer/Graph/GraphResourceHandle.h>
#include <Renderer/Graph/GraphTextureRange.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthTargetView.h>
#include <string>
#include <variant>

namespace KT::Renderer
{
	// monostateは未設定で必ず拒否。RTV/DSVの排他種別はvariantから得る。
	// 値copyしてもGPU画像/descriptor所有権は持たない。元wrapperの寿命は不要。
	// SRV/UAVは実際のGraphicsビューとheap/binding契約を導入する時に追加する。
	using GraphViewBinding = std::variant<std::monostate, KT::Graphics::ColorTargetView, KT::Graphics::DepthTargetView>;

	struct GraphViewDesc
	{
		std::string name;
		GraphResourceHandle resource{};
		GraphTextureRange range{};
		GraphViewBinding binding{};
	};
}
