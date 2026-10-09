#include <Renderer/Graph/GraphViewHandle.h>

namespace KT::Renderer
{
	bool GraphViewHandle::IsValid() const noexcept
	{
		return graphid != 0 && index != (std::numeric_limits<std::uint32_t>::max)();
	}
}
