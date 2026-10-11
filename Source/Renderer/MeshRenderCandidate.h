#pragma once
#include <Content/AssetId.h>
#include <Core/Math/AABB.h>
#include <Core/Math/Matrix4.h>
#include <vector>

namespace KT::Renderer
{
	// GPU準備前の静的Mesh候補。素材の存在確認済みで、全体に一つのMaterialを使う。
	struct MeshRenderCandidate
	{
		KT::Content::MeshAssetId mesh;
		KT::Content::MaterialAssetId material;
		KT::Core::Math::Matrix4 world;
		KT::Core::Math::AABB worldBounds;
	};

	// CameraやGPU資源の準備状態に依存しない値の集合。EntityやWorldへの参照は保持しない。
	struct RenderCandidates
	{
		std::vector<MeshRenderCandidate> objects;
	};
}
