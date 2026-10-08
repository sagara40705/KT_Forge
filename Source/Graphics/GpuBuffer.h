#pragma once
#include <Graphics/GraphicsDevice.h>
#include <cstddef>
#include <span>

namespace KT::Graphics
{
	// 初版の不変UPLOAD資源。CPU入力を構築時にコピー、GENERIC_READ固定。
	// submit/wait/描画barrierなし。所有者は記録からGPU完了まで保持する。
	// DEFAULT移行時は明示copy/transitionとstaging寿命を別のUpload責務にする。
	class GpuBuffer : private KT::Core::NonCopyable
	{
	public:
		GpuBuffer(GraphicsDevice& device, std::span<const std::byte> bytes);
		ID3D12Resource* GetResource() const noexcept { return resource_.Get(); }
		std::size_t GetSize() const noexcept { return size_; }
	private:
		ComPtr<ID3D12Resource> resource_;
		std::size_t size_=0;
	};
}
