#pragma once
#include <Core/Utility/NonCopyable.h>
#include <memory>
#include <cstddef>
#include <dxgiformat.h>

namespace Rml
{
	class RenderInterface;
}

namespace KT::Graphics
{
	class GraphicsDevice;
	class CommandQueue;
	class CommandContext;
	class ColorTargetView;
}

namespace KT::Renderer
{
	// RmlUiの基本描画を値snapshotへ捕捉し、Graphの色RTVだけへ記録する。
	// Device/Queueを借用し、Frameより長く保持する。単一thread・同期1slot専用。
	class UiRenderer : private KT::Core::NonCopyable
	{
	public:
		UiRenderer(KT::Graphics::GraphicsDevice& device, KT::Graphics::CommandQueue& queue, DXGI_FORMAT format);
		~UiRenderer();
		// RuntimeIntegrationのadapter接続用。ゲームから上流interfaceを操作しない。
		Rml::RenderInterface& Interface() noexcept;
		// Graph/Frameの記録開始前に完了待機し、Texture転送と描画列の捕捉を済ませる。
		void BeginCapture(unsigned width, unsigned height);
		void EndCapture();
		void AbortCapture() noexcept;
		// 捕捉済み列を一度記録する。barrier/Submit/Present/待機を行わない。
		void Record(KT::Graphics::CommandContext& commands, const KT::Graphics::ColorTargetView& color);
		[[nodiscard]] std::size_t DrawCount() const noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
