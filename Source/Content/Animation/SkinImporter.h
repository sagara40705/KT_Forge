#pragma once
#include <Content/Animation/SkinDefinition.h>
#include <memory>

struct aiScene;

namespace KT::Content
{
	// Assimp実体の寿命から独立したSkin入力。影響数の削減は呼出側へ診断する。
	struct ImportedSkin
	{
		std::shared_ptr<const SkinDefinition> definition;
		std::vector<SkinInfluence> influences;
		bool reducedWeights = false;
	};

	// Assimpの変換済み座標からSkeleton/Clip/offset/Weightをコピーする。
	[[nodiscard]] ImportedSkin ImportSkin(const aiScene& scene, unsigned meshIndex, float scale);
}
