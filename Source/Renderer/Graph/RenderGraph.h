#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Renderer/Graph/GraphStorage.h>
#include <Renderer/Graph/GraphCompiledPlan.h>
#include <Renderer/Graph/GraphUseResolver.h>
#include <Renderer/Graph/GraphValidator.h>
#include <Renderer/Graph/GraphCompiler.h>
#include <cstdint>
#include <optional>
#include <string>

namespace KT::Graphics { class FrameResources; }
namespace KT::Renderer
{
	class GraphExecutionContext;
	// 定義準備だけ。全処理本体は未実装。単一thread/Direct Queue/宣言順/Resetなし。
	// 内部一意IDを作りcopy/move禁止。手動IDを受け取るconstructorは設けない。
	class RenderGraph : private KT::Core::NonCopyable
	{
	public:
		// コンストラクタ
		RenderGraph();

		// 画像がこのGraphに登録されているかを確認する
		bool Contains(GraphResourceHandle resource) const;
		// ViewがこのGraphに登録されているかを確認する
		bool Contains(GraphViewHandle view) const;

		// 名前を登録してGraphResourceHandleを返す
		GraphResourceHandle RegisterResource(std::string name);
		// GPU画像をGraphへ登録してGraphResourceHandleを返す
		GraphResourceHandle ImportTexture(GraphImportedTextureDesc desc);

		GraphViewHandle AddView(GraphViewDesc desc);
		void AddPass(GraphPassDesc desc, GraphRecordFn record);

		void Validate() const;

		// 全計画をローカル完成後に公開。失敗はBuilding/登録内容を保持する。
		void Compile();
		// 記録だけ。Begin/End/Submit/Present/Waitは呼出側。同じframeのarenaを使用。
		// 記録可能Contextを取得後、全import画像とlistのDeviceをRequireSameDeviceで照合。
		// 最初の命令前に完了し、画像/descriptor対応や寿命の証明とは区別する。
		// 事前検査失敗は命令なし。入口/callback後/成功確定前にContext健全性を確認。
		// 開始後失敗はFailed/Invalidateを保持し、callbackがcatchしても成功扱いしない。
		// 失敗listは送信不可。callerのlistを勝手にResetしない。
		void Record(KT::Graphics::FrameResources& frame);

	private:
		friend class GraphExecutionContext;
		enum class State { Building, Compiled, Recording, Recorded, Failed };

		// 新しいGraphのIDを取得する。0は無効ID、1から開始する。最大値に達した場合はerror
		static std::uint64_t AcquireGraphId();

		//　登録できる状態かを確認する。Building以外は登録不可。
		void RequireBuilding() const;

		GraphStorage storage_;
		State state_ = State::Building;
		std::optional<GraphCompiledPlan> compiledPlan_;
		GraphUseResolver resolver_;
		GraphValidator validator_;
		GraphCompiler compiler_;
	};
}
