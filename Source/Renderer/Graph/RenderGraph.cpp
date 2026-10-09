#include <Renderer/Graph/RenderGraph.h>

namespace KT::Renderer
{
	// 未実装契約: 全入口を本人が実装。AcquireGraphId/constructorから開始。登録/Compile失敗は部分公開なし。Record中失敗はFailedとContext無効化。旧本体はlocal-tests原本に退避。
}
