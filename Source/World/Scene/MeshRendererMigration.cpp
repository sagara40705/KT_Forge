#include <World/Scene/MeshRendererMigration.h>
#include <stdexcept>

namespace KT::World
{
	// 旧数値をUUIDへ推測変換せず、割当がある場合だけ明示された対応表を使用する。
	template <class Id> static SceneValue ConvertReference(const SceneValue& value, const std::map<std::uint64_t, Id>& mapping)
	{
		const auto previous = value.AsUint64();
		if (previous == 0)
		{
			return {};
		}
		const auto found = mapping.find(previous);
		if (found == mapping.end())
		{
			throw std::invalid_argument("MeshRendererの旧IDに対応するAssetIdがありません: " + std::to_string(previous));
		}
		return found->second.ToString();
	}

	SceneAsset MigrateMeshRendererAssets(const SceneAsset& source, const LegacyMeshRendererAssets& assets)
	{
		if (assets.meshes.contains(0) || assets.materials.contains(0))
		{
			throw std::invalid_argument("MeshRendererの移行表へ未割当IDの0は登録できません。");
		}
		auto result = source;
		for (auto& object : result.objects)
		{
			for (auto& component : object.components)
			{
				if (component.type != "kt.MeshRenderer" || component.version != 1)
				{
					continue;
				}
				component.data.RequireFields({"meshId", "materialId", "visible"});
				const auto mesh = ConvertReference(component.data.At("meshId"), assets.meshes);
				const auto material = ConvertReference(component.data.At("materialId"), assets.materials);
				const auto visible = component.data.At("visible").AsBool();
				component.data = SceneValue::Object{{"mesh", mesh}, {"material", material}, {"visible", visible}};
				component.version = 2;
			}
		}
		return result;
	}
}
