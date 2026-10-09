#pragma once
#include <source_location>
#include <string_view>

namespace KT::Core
{
	enum class LogLevel
	{
		Info,
		Warning,
		Error,
	};

	// レベル・呼出位置・本文を標準エラー出力へ書く。出力処理を終えてから呼出元へ戻る。
	// 本文はUTF-8で渡す。このプロジェクトの/utf-8設定なら、通常の日本語文字列リテラルを使える。
	// WindowsコンソールではUnicodeに変換して表示し、コンソールのコードページは変更しない。
	// この変換では不正なUTF-8を拒否する。リダイレクト時はバイト列をそのまま書き、flushする。
	// コンソール出力はOSの標準エラーハンドルへ直接書くため、cerrの独自バッファで
	// 捕捉できるのはリダイレクト側の経路だけ。
	// 複数スレッドからのLog呼出しは1件ずつ出力する。他の処理による直接出力との混在は防げない。
	// 成功ならtrue。不正なレベル、出力・ロックの失敗、例外の発生時はfalseを返し、例外は伝えない。
	// 失敗しても自動で再試行しない。出力の一部が残ったり、cerrのエラー状態が残ったりする場合がある。
	// 本文は呼出中だけ参照するため、その間はデータを有効に保つ。本文中の改行はそのまま出力する。
	[[nodiscard]] bool Log(
		LogLevel level, std::string_view message, std::source_location location = std::source_location::current()) noexcept;
}

// 出力の成否を確認せず、短くログを書きたい場合のマクロ。成否が必要ならLogを直接呼ぶ。
// 本文の式は1回だけ評価し、呼出元の位置を記録する。Releaseでも出力処理は有効。
#define KT_LOG_INFO(message) ((void)::KT::Core::Log(::KT::Core::LogLevel::Info, (message)))

#define KT_LOG_WARNING(message) ((void)::KT::Core::Log(::KT::Core::LogLevel::Warning, (message)))

#define KT_LOG_ERROR(message) ((void)::KT::Core::Log(::KT::Core::LogLevel::Error, (message)))
