#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Graphics/Commands/FrameResources.h>
#include <d3dx12.h>
#include <cstdint>
#include <stdexcept>
#include <limits>
#include <utility>


namespace KT::Renderer
{
	// 単一スレッドで非0の一意IDを発行する。最大値で停止し、破棄したGraphのIDも再利用しない。
	std::uint64_t RenderGraph::AcquireGraphId()
	{
		static std::uint64_t currentId = 1;
		// 前検査：枯渇後も値を進めず、wrapによるIDの再発行を防ぐ。
		if (currentId == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Graph ID overflow");
		}

		// 発行：使用したIDを返し、次回の候補を進める。
		const std::uint64_t acquiredId = currentId;
		++currentId;
		return acquiredId;
	}
	// 新しいGraphの登録領域に、一意IDを設定する。
	RenderGraph::RenderGraph()
	{
		storage_.graphid = AcquireGraphId();
	}
	// 画像handleの形式・所属・範囲を確認する。不正なら例外、すべて満たす場合だけtrueを返す。
	bool RenderGraph::Contains(GraphResourceHandle resource) const
	{
		// 前検査：配列を参照する前に、handleの形式・所属・範囲を確認する。
		if (!resource.IsValid())
		{
			throw std::invalid_argument("GraphResourceHandleが無効です");
		}
		if (resource.graphid != storage_.graphid)
		{
			throw std::invalid_argument("GraphResourceHandleがこのGraphに属していません");
		}
		if (resource.index >= storage_.resources.size())
		{
			throw std::out_of_range("GraphResourceHandleのindexが範囲外です");
		}

		return true;
	}
	// view handleの形式・所属・範囲を確認する。不正なら例外、すべて満たす場合だけtrueを返す。
	bool RenderGraph::Contains(GraphViewHandle view) const
	{
		// 前検査：配列を参照する前に、handleの形式・所属・範囲を確認する。
		if (!view.IsValid())
		{
			throw std::invalid_argument("GraphViewHandleが無効です");
		}
		if (view.graphid != storage_.graphid)
		{
			throw std::invalid_argument("GraphViewHandleがこのGraphに属していません");
		}
		if (view.index >= storage_.views.size())
		{
			throw std::out_of_range("GraphViewHandleのindexが範囲外です");
		}

		return true;
	}
	// CPU宣言用の画像名を登録する。GPU画像の借用はImportTextureで別に登録する。
	GraphResourceHandle RenderGraph::RegisterResource(std::string name)
	{
		// 前検査
		RequireBuilding();
		if (name.empty())
		{
			throw std::invalid_argument("名前が空です");
		}
		if (storage_.resources.size() >= (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::overflow_error("リソースが最大数に達しました");
		}

		// 登録：縮小前に上限を確認したindexで、importなしのrecordを追加する。
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.resources.size());
		GraphResourceRecord resourceRecord{};
		resourceRecord.name = std::move(name);
		storage_.resources.push_back(std::move(resourceRecord));

		return GraphResourceHandle{ storage_.graphid, index };
	}
	// 外部のGPU画像を借用登録する。画像の所有者は最後の利用Fence完了まで保持する。
	GraphResourceHandle RenderGraph::ImportTexture(GraphImportedTextureDesc desc)
	{
		// 前検査
		RequireBuilding();
		if (desc.name.empty())
		{
			throw std::invalid_argument("名前が空です");
		}
		if (desc.resource == nullptr)
		{
			throw std::invalid_argument("resourceがnullptrです");
		}
		if (storage_.resources.size() >= (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::overflow_error("リソースが最大数に達しました");
		}

		// 重複検査：同じ画像を別の状態追跡単位として登録しない。
		for (const auto& resource : storage_.resources)
		{
			if (resource.importedTexture.has_value() && resource.importedTexture->resource == desc.resource)
			{
				throw std::invalid_argument("同じGPU画像が既に登録されています");
			}
		}

		// 登録：候補を組み立ててから追加し、失敗時は既存の登録内容を保つ。
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.resources.size());
		GraphResourceRecord resourceRecord{};
		resourceRecord.name = desc.name;
		resourceRecord.importedTexture = std::move(desc);
		storage_.resources.push_back(std::move(resourceRecord));

		return GraphResourceHandle{ storage_.graphid, index };
	}

	// Viewを登録してGraphViewHandleを返す
	GraphViewHandle RenderGraph::AddView(GraphViewDesc desc)
	{
		// 前検査
		RequireBuilding();
		if (storage_.views.size() >= (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::overflow_error("Viewが最大数に達しました");
		}
		validator_.ValidateView(desc, storage_);

		// 登録：候補を組み立ててから追加し、失敗時は既存の登録内容を保つ。
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.views.size());
		GraphViewRecord viewRecord{};
		viewRecord.desc = std::move(desc);
		storage_.views.push_back(std::move(viewRecord));

		return GraphViewHandle{ storage_.graphid, index };
	}

	// 検査済みの使用宣言と記録callbackを、実行する順に登録する。
	void RenderGraph::AddPass(GraphPassDesc desc, GraphRecordFn record)
	{
		// 前検査
		RequireBuilding();
		if (!record)
		{
			throw std::invalid_argument("GraphRecordFnがnullptrです");
		}
		validator_.ValidatePass(desc, storage_);

		// 登録：候補を組み立ててから追加し、失敗時は既存の登録内容を保つ。
		GraphPassRecord passRecord{};
		passRecord.desc = std::move(desc);
		passRecord.record = std::move(record);
		storage_.passes.push_back(std::move(passRecord));
	}

	// 登録できる状態かを確認する。Building以外は登録不可。
	void RenderGraph::RequireBuilding() const
	{
		if (state_ != State::Building)
		{
			throw std::logic_error("GraphはBuilding状態ではありません。登録操作は許可されません。");
		}
	}
	// 入力を変更せず、親画像へ正規化した使用と元宣言・内容依存を照合する。
	void RenderGraph::Validate() const
	{
		const auto resolved = resolver_.Resolve(storage_);
		validator_.Validate(storage_, resolved);
	}

	// 全計画をローカル完成後に公開。失敗はBuilding/登録内容を保持する。
	void RenderGraph::Compile()
	{
		RequireBuilding();

		// 前検査：正規化した使用と内容依存を検査してから、遷移計画を作る。
		const auto resolved = resolver_.Resolve(storage_);
		validator_.Validate(storage_, resolved);

		const auto plan = compiler_.Compile(storage_, resolved);
		// 計画の公開：保持に成功してからCompiledへ進める。
		compiledPlan_ = std::move(plan);

		state_ = State::Compiled;
	}

	// 記録だけ。Begin/End/Submit/Present/Waitは呼出側。同じframeのarenaを使用。
	void RenderGraph::Record(KT::Graphics::FrameResources& frame)
	{
		// 前検査
		if (state_ != State::Compiled)
		{
			throw std::logic_error("GraphはCompiled状態ではありません。Record操作は許可されません。");
		}
		if (!compiledPlan_.has_value())
		{
			throw std::logic_error("コンパイル計画が存在しません。");
		}
	
		// 記録先の取得：同じframeのContextと定数arenaを借りる。
		auto& commandContext = frame.GetContext();
		auto& constantBufferArena = frame.GetConstants();

		// 前検査：記録中かつ使用可能なlistを要求する。失敗時はCompiledのまま。
		if (!commandContext.GetRecordingList())
		{
			throw std::logic_error("CommandContextは記録中ではありません");
		}

		// 状態更新：事前検査後に記録を開始し、以後の例外はFailedとして保持する
		auto* commandList = commandContext.GetRecordingList();
		state_ = State::Recording;
		try
		{
			// パス記録：登録順の遷移計画をたどる。
			for (const auto& plannedPass : compiledPlan_->passes)
			{
				// 状態遷移：初版は1Mip・1slice・1planeの画像全体を対象にする
				for (const auto& transition : plannedPass.transitions)
				{
					auto resource = storage_.resources[transition.resource.index].importedTexture->resource;

					D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
						resource,
						transition.before,
						transition.after,
						D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
					);
					commandList->ResourceBarrier(1, &barrier);

				}

                // 全遷移後にcallbackを1回実行し、Contextの健全性を確認する
                GraphExecutionContext executionContext(*this, commandContext, constantBufferArena, plannedPass.passIndex);
                storage_.passes[plannedPass.passIndex].record(executionContext);

                // 失敗確認：callback内でcatchされても、無効ContextならRecordを成功させない。
                (void)commandContext.GetRecordingList();

			}

			// 終了遷移：借用画像を指定されたfinalStateへ戻す。
			for (const auto& transition : compiledPlan_->finalTransitions)
			{
				auto* resource = storage_.resources[transition.resource.index].importedTexture->resource;
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					resource,
					transition.before,
					transition.after,
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				commandList->ResourceBarrier(1, &barrier);
			}

			// 成功確定：終了遷移後もContextが健全な場合だけRecordedへ進める。
			(void)commandContext.GetRecordingList();
			state_ = State::Recorded;

		}
		// 失敗保持：記録済み命令を取り消さず、listの送信を禁止して呼出側へ伝える。
		catch (...)
		{
			commandContext.Invalidate();
			state_ = State::Failed;
			throw;
		}
	}

}
