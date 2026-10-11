#include <Graphics/Textures/SampledTexture.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Commands/CommandQueue.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>
#include <Graphics/GraphicsValidation.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace KT::Graphics
{
	SampledTexture::SampledTexture(GraphicsDevice& device, std::span<const TextureMipData> mips, DXGI_FORMAT format)
		: device_(device),
		  heap_(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true)
	{
		if (mips.empty() || mips.size() > 15 || (format != DXGI_FORMAT_R8G8B8A8_UNORM && format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
		{
			throw std::invalid_argument("SampledTextureにはRGBA8の1から15段のMipが必要です。");
		}
		auto width = mips.front().width;
		auto height = mips.front().height;
		if (width == 0 || height == 0 || width > 16384 || height > 16384)
		{
			throw std::invalid_argument("SampledTextureの寸法が1から16384の範囲外です。");
		}
		for (std::size_t level = 0; level < mips.size(); ++level)
		{
			const auto& mip = mips[level];
			if (mip.width != width || mip.height != height || mip.pixels.size() != static_cast<std::size_t>(width) * height * 4 ||
				((width == 1 && height == 1) && level + 1 != mips.size()))
			{
				throw std::invalid_argument("SampledTextureのMip寸法または画素byte数が不正です。");
			}
			width = (std::max)(1u, width / 2);
			height = (std::max)(1u, height / 2);
		}

		// GPU画像と、Deviceが要求する行pitchの転送領域を確保する。
		D3D12_RESOURCE_DESC description{};
		description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		description.Width = mips.front().width;
		description.Height = mips.front().height;
		description.DepthOrArraySize = 1;
		description.MipLevels = static_cast<UINT16>(mips.size());
		description.Format = format;
		description.SampleDesc.Count = 1;
		D3D12_HEAP_PROPERTIES properties{};
		properties.Type = D3D12_HEAP_TYPE_DEFAULT;
		properties.CreationNodeMask = properties.VisibleNodeMask = 1;
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &description,
								D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(resource_.GetAddressOf())),
			"SampledTextureのGPU画像確保に失敗");
		footprints_.resize(mips.size());
		UINT64 totalBytes = 0;
		device.GetDevice()->GetCopyableFootprints(
			&description, 0, static_cast<UINT>(mips.size()), 0, footprints_.data(), nullptr, nullptr, &totalBytes);
		D3D12_RESOURCE_DESC buffer{};
		buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer.Width = totalBytes;
		buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
		buffer.SampleDesc.Count = 1;
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &buffer,
								D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(staging_.GetAddressOf())),
			"SampledTextureの転送領域確保に失敗");
		void* mapped = nullptr;
		const D3D12_RANGE readRange{0, 0};
		CheckGraphicsResult(staging_->Map(0, &readRange, &mapped), "SampledTextureの転送領域Mapに失敗");
		for (std::size_t level = 0; level < mips.size(); ++level)
		{
			const auto& mip = mips[level];
			const auto& footprint = footprints_[level];
			for (UINT row = 0; row < mip.height; ++row)
			{
				std::memcpy(
					static_cast<std::uint8_t*>(mapped) + footprint.Offset + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
					mip.pixels.data() + static_cast<std::size_t>(row) * mip.width * 4, static_cast<std::size_t>(mip.width) * 4);
			}
		}
		staging_->Unmap(0, nullptr);
		DescriptorAllocator allocator(heap_);
		srv_ = allocator.Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = format;
		view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view.Texture2D.MipLevels = static_cast<UINT>(mips.size());
		device.GetDevice()->CreateShaderResourceView(resource_.Get(), &view, heap_.GetCpu(srv_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	}

	void SampledTexture::Upload(CommandQueue& queue)
	{
		if (ready_)
		{
			return;
		}
		RequireSameDevice(queue.GetCommandQueue(), device_.GetDevice());
		CommandContext commands(device_);
		commands.Begin();
		for (UINT level = 0; level < footprints_.size(); ++level)
		{
			D3D12_TEXTURE_COPY_LOCATION source{};
			source.pResource = staging_.Get();
			source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			source.PlacedFootprint = footprints_[level];
			D3D12_TEXTURE_COPY_LOCATION destination{};
			destination.pResource = resource_.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			destination.SubresourceIndex = level;
			commands.GetRecordingList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}
		commands.Transition(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commands.End();
		queue.ExecuteAndWait(commands);
		ready_ = true;
		staging_.Reset();
		footprints_.clear();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE SampledTexture::GetSrv() const
	{
		if (!ready_)
		{
			throw std::logic_error("SampledTextureはUpload完了前に描画へ使用できません。");
		}
		return heap_.GetGpu(srv_);
	}
}
