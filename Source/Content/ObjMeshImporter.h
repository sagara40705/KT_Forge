#pragma once
#include <Content/AssetJson.h>

namespace KT::Content
{
	// 入力座標の単位を明示変換する。既定では座標と面順をそのまま保持する。
	struct ObjMeshImportOptions
	{
		float scale = 1;
		// Z反転と三角形の面順反転を組み合わせ、座標系を変換する。
		bool flipZ = false;
		// OBJの下原点UVを画像の左上原点へ変換する。
		bool flipV = true;
		// Preserveは明示法線を保持し、欠落cornerだけOBJのs設定で生成する。
		MeshNormalMode normals = MeshNormalMode::Preserve;
		bool generateTangents = true;
	};

	// 幾何属性とMaterial slotを保持する。MTL内容とオブジェクト階層は別資源として扱う。
	struct ObjMeshImportResult
	{
		MeshAsset mesh;
		// 旧呼出側との互換用。現在はUVを保存するためfalse。
		bool omittedTexcoords = false;
		bool omittedNormals = false;
		bool omittedMaterials = false;
		bool mergedGroups = false;
	};

	struct ObjMeshSaveResult
	{
		MeshAssetId id;
		bool replaced = false;
		// 旧呼出側との互換用。現在はUVを保存するためfalse。
		bool omittedTexcoords = false;
		bool omittedNormals = false;
		bool omittedMaterials = false;
		bool mergedGroups = false;
	};

	// Wavefront OBJの三角形をCPU素材へ変換する。外部MTLや命令は実行・読込しない。
	class ObjMeshImporter
	{
	public:
		// v/vt/vnと三角形f、s/usemtlを保存する。未知命令・多角形・前方参照は拒否する。
		[[nodiscard]] static ObjMeshImportResult Parse(std::string_view text, ObjMeshImportOptions options = {});
		// 新規はUUIDを発行し、再取込では保存先MeshのUUIDと名前を維持する。
		// 全変換成功後だけAssetJsonで置換する。出力は*.asset.jsonを指定する。
		[[nodiscard]] static ObjMeshSaveResult Import(
			const std::filesystem::path& source, const std::filesystem::path& destination, ObjMeshImportOptions options = {});
		// 保存済みrecipeから元OBJと設定を解決し、成功時だけ同じUUIDの素材を置換する。
		[[nodiscard]] static ObjMeshSaveResult Reimport(const std::filesystem::path& destination);
	};
}
