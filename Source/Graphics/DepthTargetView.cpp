#include <Graphics/DepthTargetView.h>
#include <Graphics/DepthBuffer.h>
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

        // 検査
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

        const auto& desc = resource_->GetDesc();
        //Texture2D、MipLevels=1、DepthOrArraySize=1、SampleDesc.Count=1・Quality=0、Format=D32_FLOAT、ALLOW_DEPTH_STENCIL付き
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
            throw std::invalid_argument("DepthBufferのリソースがTexture2Dではありません");
        }
        if (desc.SampleDesc.Count != 1 || desc.SampleDesc.Quality != 0)
        {
            throw std::invalid_argument("DepthBufferのサンプル数が1ではありません");
        }
        if (desc.MipLevels != 1)
        {
            throw std::invalid_argument("DepthBufferのミップレベルが1ではありません");
        }
        if (desc.DepthOrArraySize != 1)
        {
            throw std::invalid_argument("DepthBufferの配列サイズが1ではありません");
        }
        if (desc.Format != DXGI_FORMAT_D32_FLOAT)
        {
            throw std::invalid_argument("DepthBufferのフォーマットがD32_FLOATではありません");
        }
        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
        {
            throw std::invalid_argument("DepthBufferのリソースフラグにALLOW_DEPTH_STENCILが設定されていません");
        }
	}

	// 保持した借用値を返す。
	ID3D12Resource* DepthTargetView::GetResource() const noexcept
	{
        return resource_;
	}

	// 保持した借用値を返す。
	D3D12_CPU_DESCRIPTOR_HANDLE DepthTargetView::GetDsv() const noexcept
	{
        return dsv_;
	}

	// 保持した借用値を返す。
	UINT DepthTargetView::GetWidth() const noexcept
	{
        return width_;
	}

	// 保持した借用値を返す。
	UINT DepthTargetView::GetHeight() const noexcept
	{
        return height_;
	}

	// 保持した借用値を返す。
	DXGI_FORMAT DepthTargetView::GetFormat() const noexcept
	{
        return format_;
	}

}
