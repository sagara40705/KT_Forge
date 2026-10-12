#include <Content/Animation/SkinImporter.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace KT::Content
{
	namespace
	{
		float Number(double value)
		{
			if (!std::isfinite(value) || std::abs(value) > (std::numeric_limits<float>::max)())
			{
				throw std::invalid_argument("骨格取込の座標・時刻を有限floatへ変換できません。");
			}
			const auto result = static_cast<float>(value);
			if (value != 0 && result == 0)
			{
				throw std::invalid_argument("骨格取込の非ゼロ値がfloatへの縮小で消失します。");
			}
			return result;
		}

		KT::Core::Math::Matrix4 Matrix(const aiMatrix4x4& source, float scale)
		{
			KT::Core::Math::Matrix4 result;
			for (std::size_t row = 0; row < 4; ++row)
			{
				for (std::size_t column = 0; column < 4; ++column)
				{
					result(row, column) = Number(
						double(source[static_cast<unsigned>(column)][static_cast<unsigned>(row)]) * (row == 3 && column < 3 ? scale : 1.0));
				}
			}
			return result;
		}
	}

	ImportedSkin ImportSkin(const aiScene& scene, unsigned meshIndex, float scale)
	{
		const auto& mesh = *scene.mMeshes[meshIndex];
		ImportedSkin result;
		if (!mesh.HasBones())
		{
			return result;
		}
		auto definition = std::make_shared<SkinDefinition>();
		std::vector<const aiNode*> nodes;
		std::vector<KT::Core::Math::Matrix4> models;
		std::map<std::string, std::uint32_t> namedJoints;
		std::optional<std::uint32_t> meshNode;

		// 骨だけでなく補助ノードを含め、アニメーションの親変換を失わない。
		const auto visit = [&](auto&& self, const aiNode& node, std::int32_t parent, unsigned depth) -> void
		{
			if (definition->joints.size() >= SkinDefinition::MaxJoints || depth > 128)
			{
				throw std::invalid_argument("Skinモデルのノード数256または階層深さ128の上限を超えています。");
			}
			const auto index = static_cast<std::uint32_t>(definition->joints.size());
			SkeletonJoint joint;
			joint.name = node.mName.length ? node.mName.C_Str() : "__node" + std::to_string(index);
			joint.parent = parent;
			aiVector3D position, scaling;
			aiQuaternion rotation;
			node.mTransformation.Decompose(scaling, rotation, position);
			joint.translation = {
				Number(double(position.x) * scale), Number(double(position.y) * scale), Number(double(position.z) * scale)};
			joint.rotation = KT::Core::Math::Normalize({rotation.x, rotation.y, rotation.z, rotation.w});
			joint.scale = {scaling.x, scaling.y, scaling.z};
			const auto local = KT::Core::Math::LocalMatrix(joint.translation, joint.rotation, joint.scale);
			const auto expected = Matrix(node.mTransformation, scale);
			for (std::size_t element = 0; element < 16; ++element)
			{
				if (std::abs(local.elements[element] - expected.elements[element]) >
					0.0001f * (std::max)(1.0f, std::abs(expected.elements[element])))
				{
					throw std::invalid_argument("SkeletonのShearをTRSへ保持できません: " + joint.name);
				}
			}
			if (!namedJoints.emplace(joint.name, index).second)
			{
				throw std::invalid_argument("Skinモデルのjoint名が重複しています: " + joint.name);
			}
			definition->joints.push_back(std::move(joint));
			nodes.push_back(&node);
			models.push_back(parent < 0 ? local : local * models[parent]);
			for (unsigned attached = 0; attached < node.mNumMeshes; ++attached)
			{
				if (node.mMeshes[attached] == meshIndex && !meshNode)
				{
					meshNode = index;
				}
			}
			for (unsigned child = 0; child < node.mNumChildren; ++child)
			{
				self(self, *node.mChildren[child], static_cast<std::int32_t>(index), depth + 1);
			}
		};
		visit(visit, *scene.mRootNode, -1, 0);
		if (!meshNode)
		{
			throw std::invalid_argument("Skin Meshがモデル階層に配置されていません。");
		}
		definition->modelToMesh = KT::Core::Math::Inverse(models[*meshNode]);
		std::vector<std::vector<std::pair<std::uint32_t, double>>> weights(mesh.mNumVertices);
		for (unsigned boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex)
		{
			const auto& bone = *mesh.mBones[boneIndex];
			const auto joint = namedJoints.find(bone.mName.C_Str());
			if (joint == namedJoints.end() || definition->bindings.size() >= SkinDefinition::MaxJoints)
			{
				throw std::invalid_argument("Skinのboneに対応するjointがないか、binding数が256を超えています。");
			}
			definition->bindings.push_back({joint->second, Matrix(bone.mOffsetMatrix, scale)});
			for (unsigned index = 0; index < bone.mNumWeights; ++index)
			{
				const auto& influence = bone.mWeights[index];
				if (influence.mVertexId >= mesh.mNumVertices || !std::isfinite(influence.mWeight) || influence.mWeight < 0)
				{
					throw std::invalid_argument("Assimp boneの頂点範囲またはWeightが不正です。");
				}
				if (influence.mWeight > 0)
				{
					weights[influence.mVertexId].emplace_back(boneIndex, influence.mWeight);
				}
			}
		}

		// 影響なしの頂点はMeshノードへ固定する。正の影響は上位4件へ正規化する。
		std::optional<std::uint32_t> rigidBinding;
		for (auto& vertex : weights)
		{
			SkinInfluence output;
			output.weights = {};
			if (vertex.empty())
			{
				if (!rigidBinding)
				{
					if (definition->bindings.size() >= SkinDefinition::MaxJoints)
					{
						throw std::invalid_argument("影響なし頂点用のSkin bindingを256件以内で追加できません。");
					}
					rigidBinding = static_cast<std::uint32_t>(definition->bindings.size());
					definition->bindings.push_back({*meshNode, {}});
				}
				output.joints[0] = *rigidBinding;
				output.weights[0] = 1;
			}
			else
			{
				std::stable_sort(
					vertex.begin(), vertex.end(), [](const auto& first, const auto& second) { return first.second > second.second; });
				if (vertex.size() > 4)
				{
					result.reducedWeights = true;
					vertex.resize(4);
				}
				double sum = 0;
				for (const auto& influence : vertex)
				{
					sum += influence.second;
				}
				for (std::size_t index = 0; index < vertex.size(); ++index)
				{
					output.joints[index] = vertex[index].first;
					output.weights[index] = Number(vertex[index].second / sum);
				}
			}
			result.influences.push_back(output);
		}

		// Assimp ticksを秒へ変換する。空チャンネルはrest値を後段のozz builderで補う。
		if (scene.mNumAnimations > 256)
		{
			throw std::invalid_argument("モデルのAnimation Clip数が256を超えています。");
		}
		for (unsigned index = 0; index < scene.mNumAnimations; ++index)
		{
			const auto& animation = *scene.mAnimations[index];
			const double ticksPerSecond = animation.mTicksPerSecond == 0 ? 25.0 : animation.mTicksPerSecond;
			if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0 || animation.mNumMeshChannels || animation.mNumMorphMeshChannels)
			{
				throw std::invalid_argument("Animationのtick速度またはMesh/Morphチャンネルが未対応です。");
			}
			AnimationClip clip;
			clip.name = animation.mName.length ? animation.mName.C_Str() : "Clip" + std::to_string(index);
			clip.duration = Number(animation.mDuration / ticksPerSecond);
			clip.tracks.resize(definition->joints.size());
			std::set<std::uint32_t> channels;
			for (unsigned channelIndex = 0; channelIndex < animation.mNumChannels; ++channelIndex)
			{
				const auto& channel = *animation.mChannels[channelIndex];
				const auto joint = namedJoints.find(channel.mNodeName.C_Str());
				if (joint == namedJoints.end() || !channels.insert(joint->second).second)
				{
					throw std::invalid_argument("Animation channelのjointが存在しないか、同じjointが重複しています。");
				}
				auto& track = clip.tracks[joint->second];
				for (unsigned keyIndex = 0; keyIndex < channel.mNumPositionKeys; ++keyIndex)
				{
					const auto& key = channel.mPositionKeys[keyIndex];
					track.translations.push_back({Number(key.mTime / ticksPerSecond),
						{Number(double(key.mValue.x) * scale), Number(double(key.mValue.y) * scale),
							Number(double(key.mValue.z) * scale)}});
				}
				for (unsigned keyIndex = 0; keyIndex < channel.mNumRotationKeys; ++keyIndex)
				{
					const auto& key = channel.mRotationKeys[keyIndex];
					track.rotations.push_back(
						{Number(key.mTime / ticksPerSecond), {key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w}});
				}
				for (unsigned keyIndex = 0; keyIndex < channel.mNumScalingKeys; ++keyIndex)
				{
					const auto& key = channel.mScalingKeys[keyIndex];
					track.scales.push_back({Number(key.mTime / ticksPerSecond), {key.mValue.x, key.mValue.y, key.mValue.z}});
				}
			}
			definition->clips.push_back(std::move(clip));
		}
		definition->Validate();
		definition->ValidateInfluences(result.influences, mesh.mNumVertices);
		result.definition = std::move(definition);
		return result;
	}
}
