#pragma once
#include <Graphics/GraphicsDevice.h>
#include <cstddef>

namespace KT::Graphics
{
	void CheckGraphicsResult(HRESULT result, const char* operation);
	void RequireSameDevice(ID3D12DeviceChild* object, ID3D12Device* expected);
	bool IsColorTargetFormat(DXGI_FORMAT format) noexcept;
	// 実データ/GPU allocationなしでlayoutサイズを検査。返り値は要素数。
	UINT ValidateVertexBufferLayout(std::size_t byteCount, UINT stride);
	UINT ValidateIndexBufferLayout(std::size_t byteCount, DXGI_FORMAT format);
}
