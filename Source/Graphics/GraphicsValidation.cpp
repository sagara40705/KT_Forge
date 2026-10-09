#include <Graphics/GraphicsValidation.h>
#include <stdexcept>
#include <limits>

namespace KT::Graphics
{
	void CheckGraphicsResult(HRESULT result, const char* operation)
	{
		// 失敗したHRESULTを例外として伝える
		if (FAILED(result))
		{
			throw std::runtime_error(operation);
		}
	}

	void RequireSameDevice(ID3D12DeviceChild* object, ID3D12Device* expectedDevice)
	{
		// 照合するオブジェクトとDeviceを確認する
		if (!object || !expectedDevice)
		{
			throw std::invalid_argument("Device comparison requires nonnull objects.");
		}

		// 資源のDeviceとCOMの同一性を確認する
		ComPtr<ID3D12Device> actualDevice;
		CheckGraphicsResult(object->GetDevice(IID_PPV_ARGS(actualDevice.GetAddressOf())), "GetDevice failed.");
		ComPtr<IUnknown> actualIdentity, expectedIdentity;
		CheckGraphicsResult(actualDevice->QueryInterface(IID_PPV_ARGS(actualIdentity.GetAddressOf())), "Device identity failed.");
		CheckGraphicsResult(expectedDevice->QueryInterface(IID_PPV_ARGS(expectedIdentity.GetAddressOf())), "Device identity failed.");
		if (actualIdentity.Get() != expectedIdentity.Get())
		{
			throw std::invalid_argument("Graphics objects belong to different devices.");
		}
	}

	bool IsColorTargetFormat(DXGI_FORMAT format) noexcept
	{
		return format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || format == DXGI_FORMAT_B8G8R8A8_UNORM ||
			format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	}

	UINT ValidateVertexBufferLayout(std::size_t byteCount, UINT stride)
	{
		// 頂点データの大きさとstrideの整合を確認する
		if (stride == 0 || stride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES || byteCount == 0 || byteCount % stride != 0)
		{
			throw std::invalid_argument("VertexBuffer bytes/stride are invalid.");
		}
		if (byteCount > (std::numeric_limits<UINT>::max)())
		{
			throw std::overflow_error("VertexBuffer view size overflow.");
		}
		return static_cast<UINT>(byteCount / stride);
	}

	UINT ValidateIndexBufferLayout(std::size_t byteCount, DXGI_FORMAT format)
	{
		// 対応するindex形式を確認する
		if (format != DXGI_FORMAT_R16_UINT && format != DXGI_FORMAT_R32_UINT)
		{
			throw std::invalid_argument("Index format must be R16_UINT/R32_UINT.");
		}

		// index形式に合う要素数とUINTの上限を確認する
		const std::size_t stride = format == DXGI_FORMAT_R16_UINT ? 2 : 4;
		if (byteCount == 0 || byteCount % stride != 0)
		{
			throw std::invalid_argument("Index bytes do not match format.");
		}
		if (byteCount > (std::numeric_limits<UINT>::max)())
		{
			throw std::overflow_error("IndexBuffer view size overflow.");
		}
		return static_cast<UINT>(byteCount / stride);
	}
}
