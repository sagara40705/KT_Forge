#pragma once
#include <World/Scene/SceneLoader.h>
#include <World/Scene/MeshRendererMigration.h>
#include <filesystem>

namespace KT::World
{
	// 標準JSONとSceneAssetの境界。WorldとRegistryへJSON型を公開しない。
	class SceneJson
	{
	public:
		// 初版の入出力上限。過大な入力と深い再帰を読込前・解析中に拒否する。
		static constexpr std::size_t MaxBytes = 64 * 1024 * 1024;
		// JSON解析と設定値の再帰変換で許可する最大階層。
		static constexpr int MaxDepth = 128;
		// 構文・保存構造・登録済み設定を検査し、Scene生成前の定義を返す。
		// 旧MeshRendererの移行は、呼出側が明示した種類別対応表がある場合だけ行う。
		[[nodiscard]] static SceneAsset Parse(std::string_view text, const ComponentRegistry& components, const ScriptRegistry& scripts,
			const LegacyMeshRendererAssets* legacyAssets = nullptr);
		// UUID・型ID・設定キーの順を安定させ、整形したUTF-8文字列を返す。
		[[nodiscard]] static std::string Stringify(
			const SceneAsset& asset, const ComponentRegistry& components, const ScriptRegistry& scripts);
		// サイズを検査してファイル全体を読み、Parseで保存定義を検査する。
		[[nodiscard]] static SceneAsset Read(const std::filesystem::path& path, const ComponentRegistry& components,
			const ScriptRegistry& scripts, const LegacyMeshRendererAssets* legacyAssets = nullptr);
		// 完成Sceneを同じディレクトリの一時ファイルへ書き、成功後に保存先を置換する。
		static void Save(
			const std::filesystem::path& path, const Scene& scene, const ComponentRegistry& components, const ScriptRegistry& scripts);
		// 制作中の定義を実体生成せず保存する。既存ファイルの保持条件はScene版と共通。
		static void Save(const std::filesystem::path& path, const SceneAsset& scene,
			const ComponentRegistry& components, const ScriptRegistry& scripts);
	};
}
