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
	// 画像の借用登録・使用検査・遷移計画・命令記録を管理する。単一スレッド・Direct Queue・宣言順。
	// 内部一意IDでhandleの所属を区別する。copy/moveとResetは行わない。
	class RenderGraph : private KT::Core::NonCopyable
	{
	public:
		// 新しいGraphの登録領域に、一意IDを設定する。
		RenderGraph();

		// 画像handleの形式・所属・範囲を確認する。不正なら例外、正常ならtrue。
		bool Contains(GraphResourceHandle resource) const;
		// view handleの形式・所属・範囲を確認する。不正なら例外、正常ならtrue。
		bool Contains(GraphViewHandle view) const;

		// CPU宣言用の画像名を登録する。GPU Compileにはimport済み画像が必要。
		GraphResourceHandle RegisterResource(std::string name);
		// 外部画像を借用登録する。画像の所有者は最後の利用Fence完了まで保持する。
		GraphResourceHandle ImportTexture(GraphImportedTextureDesc desc);

		// Viewを登録してGraphViewHandleを返す
		GraphViewHandle AddView(GraphViewDesc desc);
		// 使用宣言と記録callbackを、実行する順に登録する。
		void AddPass(GraphPassDesc desc, GraphRecordFn record);

		// 入力を変更せず、元宣言・正規化結果・内容依存を検査する。
		void Validate() const;

		// 全計画をローカル完成後に公開。失敗はBuilding/登録内容を保持する。
		void Compile();
		// 記録だけ。Begin/End/Submit/Present/Waitは呼出側。同じframeのarenaを使用。
		// Deviceは1つを前提とする。画像とdescriptorの対応・寿命は外部所有者が保証する。
		// 事前検査失敗はCompiledを保持。callback後と成功確定前にもContext健全性を確認する。
		// 開始後の例外はFailed/Invalidateを保持する。listのReset・命令の取り消しは行わない。
		void Record(KT::Graphics::FrameResources& frame);

	private:
		friend class GraphExecutionContext;
		enum class State { Building, Compiled, Recording, Recorded, Failed };

		// 非0の一意IDを発行する。最大値で停止し、破棄したGraphのIDも再利用しない。
		static std::uint64_t AcquireGraphId();

		// 登録できる状態かを確認する。Building以外は登録不可。
		void RequireBuilding() const;

		GraphStorage storage_;
		State state_ = State::Building;
		std::optional<GraphCompiledPlan> compiledPlan_;
		GraphUseResolver resolver_;
		GraphValidator validator_;
		GraphCompiler compiler_;
	};
}
