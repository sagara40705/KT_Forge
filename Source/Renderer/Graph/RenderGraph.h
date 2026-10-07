#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Renderer/Graph/RenderGraphTypes.h>
#include <Renderer/Graph/GraphImportedTexture.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>

namespace KT::Renderer
{
	// RenderGraphの本体(コピー禁止)
	class RenderGraph : private KT::Core::NonCopyable
	{
	private:
		// 自分のGraphID。0は無効
		std::uint64_t graphid_ = 0;

		// 登録したリソース名の一覧
		std::vector<std::string> resourceNames_;

		// 登録したパスの一覧
		std::vector<GraphPassDesc> passes_;

	public:
		// コンストラクタ
		explicit RenderGraph(std::uint64_t graphId): graphid_(graphId)
		{
			if (graphid_ == 0)
			{
				throw std::invalid_argument("Graph ID が無効です。");
			}
		}

		// GraphResourceHandle検査(IsValid・graphidの一致・indexの範囲)
		bool Contains(GraphResourceHandle handle) const;

		// リソースを登録し、GraphResourceHandleを返す
		GraphResourceHandle RegisterResource(std::string name);

		// パスを追加する
		void AddPass(GraphPassDesc desc);

		// Graphの検査
		void Validate() const;

	private:
		// リソースの情報
		struct ResourceRecord
		{
			// リソース名
			std::string name;
			// インポートされたテクスチャの情報
			std::optional<GraphImportedTextureDesc> importedTexture;
		};

		// 登録したリソースの情報
		std::vector<ResourceRecord> resources_;

	public:
		GraphResourceHandle ImportTexture(GraphImportedTextureDesc desc);
	};
}