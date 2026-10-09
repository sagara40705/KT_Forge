#include <Graphics/Buffers/ConstantBufferArena.h>
#include <Graphics/GraphicsValidation.h>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace KT::Graphics
{
	namespace
	{
		std::atomic<std::uint64_t> nextArenaId{1};

		std::uint64_t IssueArenaId()
		{
			// 再利用しないarenaの個体IDを発行する
			auto currentId = nextArenaId.load();
			for (;;)
			{
				if (currentId == 0 || currentId == (std::numeric_limits<std::uint64_t>::max)())
				{
					throw std::overflow_error("Constant arena ID exhausted.");
				}
				if (nextArenaId.compare_exchange_weak(currentId, currentId + 1))
				{
					return currentId;
				}
			}
		}
	}

	ConstantBufferArena::ConstantBufferArena(GraphicsDevice& device, std::size_t capacity)
		: capacity_(capacity)
	{
		// 定数領域の容量と256byte境界を確認する
		if (capacity == 0 || capacity % 256 != 0 || capacity > (std::numeric_limits<UINT>::max)())
		{
			throw std::invalid_argument("Constant arena capacity must be nonzero, 256-aligned and fit UINT.");
		}
		// GPU確保前に発行する。constructor失敗でも発行済みIDを再利用しない。
		arenaId_ = IssueArenaId();

		// UPLOADバッファの大きさと配置を設定する
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = capacity;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// UPLOADヒープに定数領域を作成する
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProperties.CreationNodeMask = heapProperties.VisibleNodeMask = 1;
		CheckGraphicsResult(device.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
								D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource_.GetAddressOf())),
			"Constant arena creation failed.");

		// GPUの参照中も保持するCPU書込先を取得する
		void* mappedData = nullptr;
		const D3D12_RANGE noRead{0, 0};
		CheckGraphicsResult(resource_->Map(0, &noRead, &mappedData), "Constant arena Map failed.");
		mapped_ = static_cast<std::byte*>(mappedData);
	}

	ConstantBufferArena::~ConstantBufferArena()
	{
		// 保持しているCPU書込先のMapを解除する
		if (mapped_)
		{
			resource_->Unmap(0, nullptr);
		}
	}

	void ConstantBufferArena::Begin()
	{
		// 再利用前の状態とepochの上限を確認する
		if (writable_)
		{
			throw std::logic_error("Constant arena is already writable.");
		}
		if (epoch_ == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Constant epoch exhausted.");
		}

		// 次の記録に使うepochと書込位置を設定する
		++epoch_;
		cursor_ = 0;
		writable_ = true;
	}

	ConstantSlice ConstantBufferArena::Write(std::span<const std::byte> bytes)
	{
		// 書込状態と入力データを確認する
		if (!writable_)
		{
			throw std::logic_error("Constant writes require an active frame.");
		}
		if (bytes.empty() || !bytes.data() || bytes.size() > 65536)
		{
			throw std::invalid_argument("Constant payload must be 1..65536 bytes.");
		}

		// 256byte境界までの必要容量を確認する
		const auto reservedBytes = (bytes.size() + 255) & ~std::size_t{255};
		if (reservedBytes > capacity_ - cursor_)
		{
			throw std::overflow_error("Constant arena is full.");
		}

		// 今回の所有者・epoch・範囲をtokenへ保持する
		ConstantSlice slice;
		slice.owner_ = this;
		slice.arenaId_ = arenaId_;
		slice.epoch_ = epoch_;
		slice.offset_ = cursor_;
		slice.size_ = bytes.size();

		// 整列領域を初期化してから入力データをコピーする
		std::memset(mapped_ + cursor_, 0, reservedBytes);
		std::memcpy(mapped_ + cursor_, bytes.data(), bytes.size());
		cursor_ += reservedBytes;
		return slice;
	}

	D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferArena::GetAddress(ConstantSlice slice) const
	{
		// 同じarenaの現在のepochと割当範囲か確認する
		if (!writable_ || slice.owner_ != this || arenaId_ == 0 || slice.arenaId_ != arenaId_ || slice.epoch_ != epoch_ ||
			slice.size_ == 0 || slice.offset_ % 256 != 0 || slice.offset_ > cursor_ || slice.size_ > cursor_ - slice.offset_)
		{
			throw std::invalid_argument("Constant slice is inactive/foreign/stale/out of range.");
		}
		return resource_->GetGPUVirtualAddress() + slice.offset_;
	}
}
