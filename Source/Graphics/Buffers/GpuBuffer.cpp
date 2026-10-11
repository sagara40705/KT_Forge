#include <Graphics/Buffers/GpuBuffer.h>
#include <Graphics/GraphicsValidation.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Commands/CommandQueue.h>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	GpuBuffer::GpuBuffer(GraphicsDevice& device, std::span<const std::byte> bytes, BufferStorage storage, D3D12_RESOURCE_STATES finalState)
		: device_(device),
		  finalState_(finalState)
	{
		// 入力データとビューに表現できる大きさを確認する
		if (bytes.empty() || !bytes.data())
		{
			throw std::invalid_argument("GpuBufferの入力byte列が空です。");
		}
		if (bytes.size() > (std::numeric_limits<UINT>::max)())
		{
			throw std::overflow_error("GpuBufferのbyte数がUINTのview上限を超えています。");
		}

		if (storage != BufferStorage::Upload && storage != BufferStorage::Default)
		{
			throw std::invalid_argument("GpuBufferの格納方式が未対応です。");
		}
		// CPU入力はstagingへコピーし、DEFAULT本体へは明示転送で反映する。
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
			"GpuBufferのUPLOAD資源生成に失敗しました。");

		// CPU書込先を取得して入力データをコピーする
		if (storage == BufferStorage::Default)
		{
			staging_ = resource_;
			resource_.Reset();
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
									D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(resource_.GetAddressOf())),
				"静的DEFAULT Bufferの生成に失敗しました。");
		}
		auto* upload = staging_ ? staging_.Get() : resource_.Get();
		void* mappedData = nullptr;
		const D3D12_RANGE noRead{0, 0};
		CheckGraphicsResult(upload->Map(0, &noRead, &mappedData), "GpuBufferのCPU書込Mapに失敗しました。");
		std::memcpy(mappedData, bytes.data(), bytes.size());

		// 書込範囲を伝えてMapを解除する
		const D3D12_RANGE written{0, bytes.size()};
		upload->Unmap(0, &written);
		size_ = bytes.size();
		ready_ = storage == BufferStorage::Upload;
	}

	void GpuBuffer::Upload(CommandQueue& queue)
	{
		if (ready_)
		{
			return;
		}
		RequireSameDevice(queue.GetCommandQueue(), device_.GetDevice());
		CommandContext commands(device_);
		commands.Begin();
		commands.Transition(resource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		commands.GetRecordingList()->CopyBufferRegion(resource_.Get(), 0, staging_.Get(), 0, size_);
		commands.Transition(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState_);
		commands.End();
		queue.ExecuteAndWait(commands);
		ready_ = true;
		staging_.Reset();
	}

}
