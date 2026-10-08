#include <Graphics/GraphicsValidation.h>
#include <stdexcept>
#include <limits>

namespace KT::Graphics
{
	void CheckGraphicsResult(HRESULT result, const char* operation)
	{
		if (FAILED(result)) throw std::runtime_error(operation);
	}
	void RequireSameDevice(ID3D12DeviceChild* object, ID3D12Device* expected)
	{
		if (!object || !expected) throw std::invalid_argument("Device comparison requires nonnull objects.");
		ComPtr<ID3D12Device> actual;
		CheckGraphicsResult(object->GetDevice(IID_PPV_ARGS(actual.GetAddressOf())),"GetDevice failed.");
		ComPtr<IUnknown> a,b;
		CheckGraphicsResult(actual->QueryInterface(IID_PPV_ARGS(a.GetAddressOf())),"Device identity failed.");
		CheckGraphicsResult(expected->QueryInterface(IID_PPV_ARGS(b.GetAddressOf())),"Device identity failed.");
		if (a.Get()!=b.Get()) throw std::invalid_argument("Graphics objects belong to different devices.");
	}
	bool IsColorTargetFormat(DXGI_FORMAT f) noexcept
	{
		return f==DXGI_FORMAT_R8G8B8A8_UNORM || f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
			f==DXGI_FORMAT_B8G8R8A8_UNORM || f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	}
	UINT ValidateVertexBufferLayout(std::size_t bytes, UINT stride)
	{
		if (stride==0 || stride>D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES || bytes==0 || bytes%stride!=0)
			throw std::invalid_argument("VertexBuffer bytes/stride are invalid.");
		if (bytes>(std::numeric_limits<UINT>::max)()) throw std::overflow_error("VertexBuffer view size overflow.");
		return static_cast<UINT>(bytes/stride);
	}
	UINT ValidateIndexBufferLayout(std::size_t bytes, DXGI_FORMAT format)
	{
		if (format!=DXGI_FORMAT_R16_UINT && format!=DXGI_FORMAT_R32_UINT) throw std::invalid_argument("Index format must be R16_UINT/R32_UINT.");
		const std::size_t stride=format==DXGI_FORMAT_R16_UINT?2:4;
		if (bytes==0 || bytes%stride!=0) throw std::invalid_argument("Index bytes do not match format.");
		if (bytes>(std::numeric_limits<UINT>::max)()) throw std::overflow_error("IndexBuffer view size overflow.");
		return static_cast<UINT>(bytes/stride);
	}
}
