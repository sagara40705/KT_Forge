#pragma once
#include <Content/AssetId.h>
#include <Core/Math/AABB.h>
#include <Core/Math/Matrix4.h>
#include <vector>
#include <memory>

namespace KT::Renderer
{
	// GPU準備前のSubmesh候補。素材の存在確認済みで、World AABBは当該範囲の境界。
	struct MeshRenderCandidate
	{
		KT::Content::MeshAssetId mesh;
		KT::Content::MaterialAssetId material;
		KT::Core::Math::Matrix4 world;
		KT::Core::Math::AABB worldBounds;
		std::uint32_t submesh = 0;
		// 個体ごとの完成Skin行列を値所有で共有する。静的Meshは空。
		std::shared_ptr<const std::vector<KT::Core::Math::Matrix4>> skinPalette;
		// falseのSkin境界はbind pose由来。変形後のカリングや配置判定へ使わない。
		bool boundsMatchPose = true;
	};

	// CameraやGPU資源の準備状態に依存しない値の集合。EntityやWorldへの参照は保持しない。
	struct RenderCandidates
	{
		std::vector<MeshRenderCandidate> objects;
	};
}
