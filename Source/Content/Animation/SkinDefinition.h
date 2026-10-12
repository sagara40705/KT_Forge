#pragma once
#include <Core/Math/Matrix4.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KT::Content
{
	// GPU頂点ごとの影響。jointはSkeleton位置ではなくbindingsの位置を指す。
	struct SkinInfluence
	{
		std::array<std::uint32_t, 4> joints{};
		std::array<float, 4> weights{1, 0, 0, 0};
	};

	// 親先行の骨格入力。親なしは-1、Transformは親の座標系で保持する。
	struct SkeletonJoint
	{
		std::string name;
		std::int32_t parent = -1;
		KT::Core::Math::Vector3 translation{};
		KT::Core::Math::Quaternion rotation{};
		KT::Core::Math::Vector3 scale{1, 1, 1};
	};

	// Meshの座標からjointのbind座標へ変換する。Meshごとのoffsetを共有骨格から分離する。
	struct SkinBinding
	{
		std::uint32_t joint = 0;
		KT::Core::Math::Matrix4 inverseBind;
	};

	// 秒単位のkey。チャンネル内のtimeは厳密な昇順とする。
	struct VectorKey
	{
		float time = 0;
		KT::Core::Math::Vector3 value{};
	};

	struct RotationKey
	{
		float time = 0;
		KT::Core::Math::Quaternion value{};
	};

	// 未指定チャンネルはjointのrest値を使用する。
	struct JointTrack
	{
		std::vector<VectorKey> translations;
		std::vector<RotationKey> rotations;
		std::vector<VectorKey> scales;
	};

	// 一つのSkeleton全jointに対応するClip。共有素材に再生時刻を保持しない。
	struct AnimationClip
	{
		std::string name;
		float duration = 1;
		std::vector<JointTrack> tracks;
	};

	// Mesh素材に付随する不変の骨格・Clip入力。ozz型とGPU資源は保持しない。
	struct SkinDefinition
	{
		static constexpr std::size_t MaxJoints = 256;
		static constexpr std::size_t MaxKeys = 1000000;
		std::vector<SkeletonJoint> joints;
		std::vector<SkinBinding> bindings;
		std::vector<AnimationClip> clips;
		// モデル全体で評価した骨行列を、このMeshノードのbind座標へ戻す。
		KT::Core::Math::Matrix4 modelToMesh;

		void Validate() const;
		void ValidateInfluences(const std::vector<SkinInfluence>& influences, std::size_t vertexCount) const;
	};
}
