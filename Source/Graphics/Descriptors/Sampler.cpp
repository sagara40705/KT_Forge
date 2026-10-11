#include <Graphics/Descriptors/Sampler.h>
#include <Graphics/Descriptors/DescriptorAllocator.h>
#include <stdexcept>

namespace KT::Graphics
{
	Sampler::Sampler(GraphicsDevice& device, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressU, D3D12_TEXTURE_ADDRESS_MODE addressV)
		: heap_(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, true)
	{
		if ((filter != D3D12_FILTER_MIN_MAG_MIP_POINT && filter != D3D12_FILTER_MIN_MAG_MIP_LINEAR) ||
			(addressU != D3D12_TEXTURE_ADDRESS_MODE_WRAP && addressU != D3D12_TEXTURE_ADDRESS_MODE_CLAMP) ||
			(addressV != D3D12_TEXTURE_ADDRESS_MODE_WRAP && addressV != D3D12_TEXTURE_ADDRESS_MODE_CLAMP))
		{
			throw std::invalid_argument("SamplerにはPoint/LinearとWrap/Clampが必要です。");
		}
		DescriptorAllocator allocator(heap_);
		sampler_ = allocator.Allocate();
		D3D12_SAMPLER_DESC description{};
		description.Filter = filter;
		description.AddressU = addressU;
		description.AddressV = addressV;
		description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		description.MaxAnisotropy = 1;
		description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		description.MaxLOD = D3D12_FLOAT32_MAX;
		device.GetDevice()->CreateSampler(&description, heap_.GetCpu(sampler_, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));
	}
}
