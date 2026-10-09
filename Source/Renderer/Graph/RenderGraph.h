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
		RenderGraph();
		bool Contains(GraphResourceHandle resource) const;
		bool Contains(GraphViewHandle view) const;
		GraphResourceHandle RegisterResource(std::string name); // CPU宣言のみ。GPU Compileはimport必須。
		GraphResourceHandle ImportTexture(GraphImportedTextureDesc desc);
		GraphViewHandle AddView(GraphViewDesc desc);
		void AddPass(GraphPassDesc desc, GraphRecordFn record);
		void Validate() const;
		void Compile();
		// 記録だけ。Begin/End/Submit/Present/Waitは呼出側。同じframeのarenaを使用。
		void Record(KT::Graphics::FrameResources& frame);
	private:
		friend class GraphExecutionContext;
		enum class State { Building, Completed, Recording, Recorded, Failed };
		// 未実装: プロセス内で非0/再発行なし。最大値を枯渇用に予約し循環せずoverflow_error。
		static std::uint64_t AcquireGraphId();
		void RequireBuilding() const;
		GraphStorage storage_;
		State state_ = State::Building;
		std::optional<GraphCompiledPlan> compiledPlan_;
		GraphUseResolver resolver_;
		GraphValidator validator_;
		GraphCompiler compiler_;
	};
}
