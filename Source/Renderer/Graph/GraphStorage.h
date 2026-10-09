#pragma once
#include <Renderer/Graph/GraphImportedTexture.h>
#include <Renderer/Graph/GraphView.h>
#include <Renderer/Graph/GraphPass.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace KT::Renderer
{
	struct GraphResourceRecord
	{
		std::string name;
		// CPU宣言用RegisterResourceは空。GPU Compileはimport済みを要求。
		std::optional<GraphImportedTextureDesc> importedTexture;
	};

	struct GraphViewRecord
	{
		GraphViewDesc desc;
	};

	struct GraphPassRecord
	{
		GraphPassDesc desc;
		GraphRecordFn record;
	};

	// RenderGraphがprivateに所有する登録正本。外部へ可変参照を公開しない。
	// GPU画像/descriptor/Renderer描画フレームを所有しない。
	struct GraphStorage
	{
		std::uint64_t graphid = 0;
		std::vector<GraphResourceRecord> resources;
		std::vector<GraphViewRecord> views;
		std::vector<GraphPassRecord> passes;
	};
}
