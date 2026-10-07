#pragma once
#include <Core/Utility/NonCopyable.h>
#include<Graphics/GraphicsDevice.h>

namespace KT::Graphics
{
	// AllocatorとListを所有。「命令の記録」を担当する
	class CommandContext : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ
		explicit CommandContext(GraphicsDevice& device);

		// デストラクタ
		~CommandContext() = default;

	private:
		ComPtr<ID3D12CommandAllocator> commandAllocator_{};
		ComPtr<ID3D12GraphicsCommandList> commandList_{};
	};
}
