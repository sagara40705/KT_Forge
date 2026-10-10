#include <World/Scene/ScriptComponent.h>
#include <stdexcept>
#include <typeinfo>

namespace KT::World
{
	std::unique_ptr<RollbackState> ScriptBehaviour::CaptureRollback()
	{
		throw std::logic_error(std::string("Scriptの更新にはCaptureRollbackの実装が必要です: ") + typeid(*this).name());
	}

	std::unique_ptr<RollbackState> ScriptComponent::CaptureRollback()
	{
		// 配列の所有・設定は不変なので、変更を許す有効設定だけを保存する。
		// 同じ実体のenabled配列へ、確保なしで値を戻す。
		class EnabledRollback final : public RollbackState
		{
		public:
			explicit EnabledRollback(std::vector<ScriptEntry>& entries)
				: entries_(entries)
			{
				enabled_.reserve(entries.size());
				for (const auto& entry : entries)
				{
					enabled_.push_back(entry.definition.enabled);
				}
			}

			void Restore() noexcept override
			{
				for (std::size_t index = 0; index < entries_.size(); ++index)
				{
					entries_[index].definition.enabled = enabled_[index];
				}
			}

		private:
			// 構造変更journalが、復元終了までこの配列の実体を保持する。
			std::vector<ScriptEntry>& entries_;
			std::vector<bool> enabled_;
		};
		return std::make_unique<EnabledRollback>(entries_);
	}

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
