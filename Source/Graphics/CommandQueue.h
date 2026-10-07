#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Graphics/GraphicsDevice.h>
#include <cstdint>

namespace KT::Graphics
{
	// コマンドキューを管理するクラス
	class CommandQueue : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ
		explicit CommandQueue(GraphicsDevice& device);

		// デスコンストラクタ
		~CommandQueue() = default;

	private:
		// コマンドキュー
		ComPtr<ID3D12CommandQueue> queue_{};

		// Fence
		ComPtr<ID3D12Fence> fence_{};

	public:
		// Fenceの値を進める
		std::uint64_t Signal();

		// 指定した目印までGPUが完了したか
		bool IsComplete(std::uint64_t fenceValue) const;

		// 指定した目印までGPUが完了するまで待つ
		void Wait(std::uint64_t fenceValue) const;
	private:
		// 次のFenceの値
		std::uint64_t nextFenceValue_ = 1;
	};
}