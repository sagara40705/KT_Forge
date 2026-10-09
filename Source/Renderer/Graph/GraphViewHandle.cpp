#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	// GraphのViewの非所有識別子。Viewとは別のindex。Graph破棄後は使用不可、IDは再発行しない。
	struct GraphViewHandle
	{
		// 所属するGraphのID
		std::uint64_t graphid = 0;

		// Graph内でのindex

		std::uint32_t index = (std::numeric_limits<std::uint32_t>::max)();
		// このHandleが有効化か
		bool IsValid() const noexcept
		{
			return graphid != 0 && index != (std::numeric_limits<std::uint32_t>::max)();
		}
	};
}
