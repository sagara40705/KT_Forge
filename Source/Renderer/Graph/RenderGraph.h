#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Renderer/Graph/RenderGraphTypes.h>
#include <Renderer/Graph/GraphImportedTexture.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <functional>
#include <cstddef>

namespace KT::Renderer
{
	// 前方宣言
	class GraphExecutionContext;

	//「GraphExecutionContextを受け取り、戻り値なしで命令を記録する処理」を保持する
	using GraphRecordFn = std::function<void(GraphExecutionContext&)>;

	// RenderGraphの本体(コピー禁止)
	class RenderGraph : private KT::Core::NonCopyable
	{
	private:
		// 自分のGraphID。0は無効
		std::uint64_t graphid_ = 0;

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

	private:
		// パスをまとめて保持するための内部構造体
		struct PassRecord
		{
			// パスのDesc
			GraphPassDesc desc;
			// 命令を記録する関数
			GraphRecordFn record;
		};
		// 登録したパスの一覧
		std::vector<PassRecord> passes_;

	private:
		// Graphの状態
		enum class State
		{
			Building,	// リソースとパスを登録できる
			Completed,	// 検証・計画作成が完了し、登録内容が固定された
			Recording,	// CommandListへ命令を記録している途中
			Recorded,	// Graphの命令記録が完了した
			Failed,		// 記録に失敗し、このGraphを再利用できない
		};
		// Graphの現在の状態
		State state_ = State::Building;
		// Building状態であることを検査し、違う場合はstd::logic_errorをthrowする
		void RequireBuilding() const;

	private:
		// 状態遷移1件の計画
		struct PlannedTransition
		{
			GraphResourceHandle resource;	// どの画像を
			D3D12_RESOURCE_STATES before;	// どの状態から
			D3D12_RESOURCE_STATES after;	// どの状態へ変えるか
		};
		// パス1件の計画
		struct PlannedPass
		{
			std::size_t passIndex = 0;						// passes_の何番目のパスか
			std::vector<PlannedTransition> transitions{};	// このパスで行う状態遷移の一覧
		};
		// Graph全体の計画
		struct CompiledPlan
		{
			std::vector<PlannedPass> passes{};					// パスの計画一覧
			std::vector<PlannedTransition> finalTransitions{};	// Import時のfinalStateへ戻すための遷移
		};

		std::optional<CompiledPlan> compiledPlan_ = std::nullopt;	// 計画が作成されていれば保持する

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
		void AddPass(GraphPassDesc desc, GraphRecordFn record);

		// Graphの検査
		void Validate() const;

		// 外部からインポートされたテクスチャを登録し、GraphResourceHandleを返す
		GraphResourceHandle ImportTexture(GraphImportedTextureDesc desc);

		// Graphの計画を作成する
		void Compile();
	};
}