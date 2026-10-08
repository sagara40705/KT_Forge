#pragma once
#include <Graphics/GraphicsDevice.h>

namespace KT::Graphics
{
	enum class TextureUsage { RenderTarget, Depth };
	// DEFAULT画像だけを所有、descriptorを所有しない。COMMONで作成、描画barrierは呼出側。
	// 初版は2D/1mip/1slice/sample1、RGBA/BGRA8またはD32_FLOATだけ。
	class Texture : private KT::Core::NonCopyable
	{
	public:
		Texture(GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format, TextureUsage usage);
		ID3D12Resource* GetResource() const noexcept { return resource_.Get(); }
		UINT GetWidth() const noexcept { return width_; }
		UINT GetHeight() const noexcept { return height_; }
		DXGI_FORMAT GetFormat() const noexcept { return format_; }
	private:
		ComPtr<ID3D12Resource> resource_;
		UINT width_,height_;
		DXGI_FORMAT format_;
	};
}
