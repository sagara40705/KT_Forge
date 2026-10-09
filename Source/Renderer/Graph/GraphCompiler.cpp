#include <Renderer/Graph/GraphCompiler.h>
#include <stdexcept>

namespace KT::Renderer
{
    GraphCompiledPlan GraphCompiler::Compile(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const
    {
		GraphCompiledPlan plan{};
        std::vector<D3D12_RESOURCE_STATES> currentStates{};

		for (const auto& resourceRecord : storage.resources)
		{
			if (!resourceRecord.importedTexture.has_value())
			{
				throw std::invalid_argument("リソースがインポートされていません");
			}

			// 初期状態をcurrentStatesに追加
			currentStates.push_back(resourceRecord.importedTexture->initialState);

		}

    }
}
