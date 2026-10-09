#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	// handleの形式だけを検査する。Graphへの所属や現在の登録範囲は保証しない。
	bool GraphViewHandle::IsValid() const noexcept
	{
		return graphid != 0 && index != (std::numeric_limits<std::uint32_t>::max)();
	}
}
