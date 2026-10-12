#pragma once
#include <Graphics/Textures/Texture.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>
#include <memory>

namespace KT::Graphics
{
	// D32_FLOAT画像と専用DSV heapを所有。Reverse-Z clear0、PSO比較GREATER。
	class DepthBuffer : private KT::Core::NonCopyable
	{
	public:
		DepthBuffer(
			GraphicsDevice& device, UINT width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT, bool shaderReadable = false);

		const Texture& GetTexture() const noexcept
		{
			return texture_;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const
		{
			return heap_.GetCpu(handle_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		// 深度判定だけを行う描画用DSV。同じ画像を所有し、書込みを禁止する。
		D3D12_CPU_DESCRIPTOR_HANDLE GetReadOnlyDsv() const
		{
			return heap_.GetCpu(readOnlyHandle_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		// 読取対応で生成した画像だけに有効なCPU SRV。GPU tableは別所有者が作る。
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrv() const;

		[[nodiscard]] ID3D12DescriptorHeap* GetSrvHeap() const noexcept
		{
			return srvHeap_ ? srvHeap_->GetHeap() : nullptr;
		}

	private:
		Texture texture_;
		DescriptorHeap heap_;
		DescriptorAllocator allocator_;
		DescriptorHandle handle_;
		// 通常DSVと同じheapに置くreadonly descriptor。
		DescriptorHandle readOnlyHandle_;
		// SRVはCPU専用heapへ置き、描画ごとのtable生成に使う。
		std::unique_ptr<DescriptorHeap> srvHeap_;
		DescriptorHandle srvHandle_;
	};
}
