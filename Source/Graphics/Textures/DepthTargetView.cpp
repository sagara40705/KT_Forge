#include <Graphics/Textures/DepthTargetView.h>
#include <Graphics/Textures/DepthBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	// DepthBufferの画像・DSV・寸法・formatを検査し、借用値として保持する。
	DepthTargetView::DepthTargetView(const DepthBuffer& owner)
	{
		const auto& texture = owner.GetTexture();
		resource_ = texture.GetResource();
		dsv_ = owner.GetDsv();
		width_ = texture.GetWidth();
		height_ = texture.GetHeight();
		format_ = texture.GetFormat();

		// 借用した画像・DSV・寸法・formatを確認する
		if (!resource_)
		{
			throw std::invalid_argument("DepthBufferのリソースが無効です");
		}
		if (dsv_.ptr == 0)
		{
			throw std::invalid_argument("DepthBufferのDSVが無効です");
		}
		if (width_ == 0 || height_ == 0)
		{
			throw std::invalid_argument("DepthBufferの幅または高さが0です");
		}
		if (format_ != DXGI_FORMAT_D32_FLOAT)
		{
			throw std::invalid_argument("DepthBufferのフォーマットがD32_FLOATではありません");
		}

		// 2D・1mip・1slice・sample1の書込可能な深度画像か確認する
		const auto& resourceDesc = resource_->GetDesc();
		if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			throw std::invalid_argument("DepthBufferのリソースがTexture2Dではありません");
		}
		if (resourceDesc.SampleDesc.Count != 1 || resourceDesc.SampleDesc.Quality != 0)
		{
			throw std::invalid_argument("DepthBufferのサンプル数が1ではありません");
		}
		if (resourceDesc.MipLevels != 1)
		{
			throw std::invalid_argument("DepthBufferのミップレベルが1ではありません");
		}
		if (resourceDesc.DepthOrArraySize != 1)
		{
			throw std::invalid_argument("DepthBufferの配列サイズが1ではありません");
		}
		if (resourceDesc.Format != DXGI_FORMAT_D32_FLOAT)
		{
			throw std::invalid_argument("DepthBufferのフォーマットがD32_FLOATではありません");
		}
		if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
		{
			throw std::invalid_argument("DepthBufferのリソースフラグにALLOW_DEPTH_STENCILが設定されていません");
		}
	}

	ID3D12Resource* DepthTargetView::GetResource() const noexcept
	{
		return resource_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DepthTargetView::GetDsv() const noexcept
	{
		return dsv_;
	}

	UINT DepthTargetView::GetWidth() const noexcept
	{
		return width_;
	}

	UINT DepthTargetView::GetHeight() const noexcept
	{
		return height_;
	}

	DXGI_FORMAT DepthTargetView::GetFormat() const noexcept
	{
		return format_;
	}

}
