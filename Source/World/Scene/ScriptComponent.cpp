#include <World/Scene/ScriptComponent.h>
#include <stdexcept>

namespace KT::World
{
	ScriptComponent::ScriptComponent(std::vector<ScriptEntry> entries)
		: entries_(std::move(entries))
	{
		// クラス名を検査する。未登録Scriptは実体なしの設定として保持する。
		for (const auto& entry : entries_)
		{
			if (entry.definition.className.empty())
			{
				throw std::invalid_argument("Script設定のクラス名が空です。");
			}
		}
	}
}
