#pragma once

namespace KT::Core
{
	// コピー禁止の基底クラス
	class NonCopyable
	{
	protected:
		NonCopyable() = default;
		~NonCopyable() = default;
	public:
		// コピーコンストラクタとコピー代入演算子を削除
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;

		// ムーブも禁止
		NonCopyable(NonCopyable&&) = delete;
		NonCopyable& operator=(NonCopyable&&) = delete;
	};
}