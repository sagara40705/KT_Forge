#include <Graphics/Buffers/GpuBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	GpuBuffer::GpuBuffer(GraphicsDevice& device, std::span<const std::byte> bytes)
	{
		// 入力データとビューに表現できる大きさを確認する
		if (bytes.empty() || !bytes.data())
		{
			throw std::invalid_argument("GpuBuffer input is empty.");
		}
		if (bytes.size() > (std::numeric_limits<UINT>::max)())
		{
			throw std::overflow_error("GpuBuffer exceeds UINT view size.");
		}

		// 不変データを保持するUPLOADヒープを設定する
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProperties.CreationNodeMask = heapProperties.VisibleNodeMask = 1;

		// バッファの大きさと配置を設定する
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = bytes.size();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// GENERIC_READ状態でバッファを作成する
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
								D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource_.GetAddressOf())),
			"GpuBuffer creation failed.");

		// CPU書込先を取得して入力データをコピーする
		void* mappedData = nullptr;
		const D3D12_RANGE noRead{0, 0};
		CheckGraphicsResult(resource_->Map(0, &noRead, &mappedData), "GpuBuffer Map failed.");
		std::memcpy(mappedData, bytes.data(), bytes.size());

		// 書込範囲を伝えてMapを解除する
		const D3D12_RANGE written{0, bytes.size()};
		resource_->Unmap(0, &written);
		size_ = bytes.size();
	}
}
