#include <Graphics/Textures/ColorDepthTextureTable.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>
#include <Graphics/GraphicsValidation.h>
#include <stdexcept>

namespace KT::Graphics
{
	ColorDepthTextureTable::ColorDepthTextureTable(GraphicsDevice& device, const SampledTexture* color, PixelDepthView depth)
		: heap_(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, true),
		  color_(color),
		  depth_(depth.GetResource())
	{
		RequireSameDevice(depth_, device.GetDevice());
		RequireSameDevice(depth.GetHeap(), device.GetDevice());
		if (color && !color->IsReady())
		{
			throw std::invalid_argument("ColorDepthTextureTableの色Texture転送が完了していません。");
		}
		DescriptorAllocator allocator(heap_);
		first_ = allocator.Allocate();
		const auto second = allocator.Allocate();
		// 色がないMaterialでも有効なnull SRVを置き、Shader分岐でSampleを省く。
		D3D12_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view.Texture2D.MipLevels = 1;
		if (color)
		{
			RequireSameDevice(color->GetResource(), device.GetDevice());
			const auto desc = color->GetResource()->GetDesc();
			view.Format = desc.Format;
			view.Texture2D.MipLevels = desc.MipLevels;
		}
		device.GetDevice()->CreateShaderResourceView(
			color ? color->GetResource() : nullptr, &view, heap_.GetCpu(first_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		device.GetDevice()->CopyDescriptorsSimple(
			1, heap_.GetCpu(second, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), depth.GetSrv(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
