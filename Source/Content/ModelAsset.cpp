#include <Content/ModelAsset.h>
#include <Content/AssetCatalog.h>
#include <Core/Math/Matrix4.h>
#include <unordered_set>
#include <stdexcept>
#include <utility>

namespace KT::Content
{
	ModelAsset::ModelAsset(std::vector<ModelNode> nodes)
		: nodes_(std::move(nodes))
	{
		Validate();
	}

	ModelAsset& ModelAsset::operator=(ModelAsset other) noexcept
	{
		nodes_.swap(other.nodes_);
		return *this;
	}

	void ModelAsset::Validate() const
	{
		if (nodes_.empty() || nodes_.size() > MaxNodes)
		{
			throw std::invalid_argument("Modelのノード数には1..10万が必要です。");
		}
		std::unordered_set<std::string> keys;
		std::unordered_set<AssetId, AssetIdHash> ids;
		std::vector<std::size_t> depths(nodes_.size());
		for (std::size_t index = 0; index < nodes_.size(); ++index)
		{
			const auto& node = nodes_[index];
			if (!ids.insert(node.id.Value()).second)
			{
				throw std::invalid_argument("Modelの永続ノードIDが重複しています。");
			}
			if (node.key.empty() || node.key.find('\0') != std::string::npos || !keys.insert(node.key).second)
			{
				throw std::invalid_argument("Modelノードのkeyは空・NUL・重複を許可しません。");
			}
			if (node.parent)
			{
				if (*node.parent >= index)
				{
					throw std::invalid_argument("Modelノードの親には同じ配列の先行ノードが必要です: " + node.key);
				}
				depths[index] = depths[*node.parent] + 1;
			}
			if (depths[index] > MaxDepth)
			{
				throw std::invalid_argument("Modelの階層深さが128を超えています。");
			}
			// Sceneと同じTRS演算で有限値と非ゼロQuaternionを検査する。
			(void)KT::Core::Math::LocalMatrix(node.position, node.rotation, node.scale);
			if (!node.mesh && (node.material || !node.materials.empty()))
			{
				throw std::invalid_argument("Meshを持たないModelノードへMaterialを割り当てられません: " + node.key);
			}
			for (const auto& [slot, material] : node.materials)
			{
				if (slot.empty() || slot.find('\0') != std::string::npos)
				{
					throw std::invalid_argument("ModelのMaterial slot名は空またはNULを許可しません。");
				}
			}
		}
	}

	std::span<const ModelNode> ModelAsset::Nodes() const noexcept
	{
		return nodes_;
	}

	void ModelAsset::ValidateReferences(const AssetCatalog& catalog) const
	{
		if (!catalog.IsFrozen())
		{
			throw std::logic_error("Model参照の検査前にAssetCatalogをFreezeしてください。");
		}
		Validate();
		for (const auto& node : nodes_)
		{
			if (!node.mesh)
			{
				continue;
			}
			const auto mesh = catalog.Resolve(node.mesh);
			if (mesh.state != AssetReferenceState::Ready)
			{
				throw std::invalid_argument("ModelのMesh参照が欠落または種類違いです: " + node.mesh->ToString());
			}
			std::unordered_set<std::string> slots;
			for (const auto& submesh : mesh.asset->Submeshes())
			{
				slots.insert(submesh.materialSlot);
			}
			for (const auto& [slot, material] : node.materials)
			{
				if (!slots.contains(slot))
				{
					throw std::invalid_argument("ModelのMaterial割当先がMeshにありません: " + node.key + " / " + slot);
				}
			}
			// 未割当は描画しない。指定したMaterialはfallbackを含め存在を要求する。
			std::vector<std::optional<MaterialAssetId>> materials{node.material};
			for (const auto& [slot, material] : node.materials)
			{
				materials.push_back(material);
			}
			for (const auto id : materials)
			{
				if (!id)
				{
					continue;
				}
				const auto material = catalog.Resolve(id);
				if (material.state != AssetReferenceState::Ready)
				{
					throw std::invalid_argument("ModelのMaterial参照が欠落または種類違いです: " + id->ToString());
				}
				if (material.asset->Settings().baseColorTexture &&
					catalog.Resolve(material.asset->Settings().baseColorTexture).state != AssetReferenceState::Ready)
				{
					throw std::invalid_argument("ModelのMaterialが参照するTextureが欠落または種類違いです: " + id->ToString());
				}
			}
			// 実際に描くslotだけを照合し、明示nullで抑止した範囲へUVを要求しない。
			for (const auto& slot : slots)
			{
				const auto assigned = node.materials.find(slot);
				const auto id = assigned == node.materials.end() ? node.material : assigned->second;
				if (id && catalog.Resolve(id).asset->Settings().baseColorTexture && mesh.asset->Texcoords().empty())
				{
					throw std::invalid_argument("ModelのTexture付きMaterialに必要なMeshのUV0がありません: " + node.key + " / " + slot);
				}
			}
		}
	}
}
