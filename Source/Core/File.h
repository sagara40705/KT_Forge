#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace KT::Core::File
{
	// 通常ファイルを先頭から末尾まで読み、データを所有するvectorを返す。空ファイルも成功する。
	// サイズ取得と先頭への移動ができるファイルが対象。パスは有効なUnicodeを前提とし、
	// Windowsの日本語パスもfilesystem::pathで受け取る。相対パスの基準は現在の作業ディレクトリ。
	// パス・読込・サイズの問題は、UTF-8のパスと原因を含むruntime_errorで知らせる。
	// メモリ確保の失敗にも可能な範囲で原因を添えるが、診断文自体を確保できない場合は
	// メモリ確保の例外がそのまま伝わる。失敗時に読込途中のデータを返すことはない。
	// 読込中のファイル変更は対応範囲外。縮小や検出できた増大は失敗として扱うが、
	// ファイルをロックしていないため、ある一時点の内容を取得した保証はない。
	[[nodiscard]] std::vector<std::byte> ReadBinary(const std::filesystem::path& path);

	// ReadBinaryと同じ読込条件で、ファイルのバイト列をstringに格納して返す。
	// 「Text」でも文字コードの検証・変換は行わない。BOM・改行・途中のNULも元のまま残す。
	// string内の文字数や文字コードを解釈する処理は呼出側で行う。読込データに終端文字は追加しない。
	[[nodiscard]] std::string ReadText(const std::filesystem::path& path);
}
