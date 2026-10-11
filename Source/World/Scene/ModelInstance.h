#pragma once
#include <Content/AssetId.h>
#include <World/Scene/SceneAsset.h>
#include <set>

namespace KT::World
{
	// 原本との対応を外したノードも記録し、次回反映で再生成しない。
	enum class ModelNodeState
	{
		Present,
		Suppressed,
		Detached
	};

	// 個体の値は各Componentが正本。ここには前回原本と照合・編集方針だけを保存する。
	struct ModelNodeBinding
	{
		KT::Content::ModelNodeId node;
		ObjectUuid object;
		ModelNodeState state = ModelNodeState::Present;
		// name/parent/transform/rendererの前回原本値。parentはModelNodeIdで保持する。
		SceneValue source;
		// name/parent/position/rotation/scale/mesh/material/visible/materials:<slot>。
		std::set<std::string> overrides;
	};

	// 専用の配置rootへ付ける。Entityや資源実体を持たず、Scene保存で継続する。
	struct ModelInstance
	{
		KT::Content::ModelAssetId model;
		// 最後に適用したModelと依存資源の内容Revision。
		std::string revision;
		std::vector<ModelNodeBinding> nodes;
		[[nodiscard]] SceneValue Encode() const;
		[[nodiscard]] static ModelInstance Decode(const SceneValue& value);
		// 明示overrideの対象を限定し、誤記による編集の消失を防ぐ。
		static void ValidateOverride(std::string_view field);
	};
}
