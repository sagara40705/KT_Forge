#include <Renderer/Graph/RenderGraph.h>
#include <Renderer/Graph/GraphExecutionContext.h>
#include <Graphics/FrameResources.h>
#include <d3dx12.h>
#include <cstdint>
#include <stdexcept>
#include <limits>
#include <utility>


namespace KT::Renderer
{
	// 新しいGraphのIDを取得する。0は無効ID、1から開始する。最大値に達した場合はerror。
	std::uint64_t RenderGraph::AcquireGraphId()
	{
		static std::uint64_t currentId = 1; // 0は無効IDとして予約
		if (currentId == (std::numeric_limits<std::uint64_t>::max)())
		{
			throw std::overflow_error("Graph ID overflow");
		}

		// 現在のIDを返し、次のIDに進める
		const std::uint64_t acquiredId = currentId;
		++currentId;
		return acquiredId;
	}
	// コンストラクタ
	RenderGraph::RenderGraph()
	{
		// 新しいGraphのIDを取得してstorage_に設定
		storage_.graphid = AcquireGraphId();
	}
	// 画像がこのGraphに登録されているかを確認する
	bool RenderGraph::Contains(GraphResourceHandle resource) const
	{
		// 検査
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
	// ViewがこのGraphに登録されているかを確認する
	bool RenderGraph::Contains(GraphViewHandle view) const
	{
		// 検査
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
	// 名前を登録してGraphResourceHandleを返す
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

		// 新しいリソースを登録(ImportTextureは未設定)
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.resources.size());
		GraphResourceRecord resourceRecord{};
		resourceRecord.name = std::move(name);
		storage_.resources.push_back(std::move(resourceRecord));

		// GraphResourceHandleを返す
		return GraphResourceHandle{ storage_.graphid, index };
	}
	// GPU画像をGraphへ登録してGraphResourceHandleを返す
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

		// 同じ画像の二重登録を防ぐ
		for (const auto& resource : storage_.resources)
		{
			if (resource.importedTexture.has_value() && resource.importedTexture->resource == desc.resource)
			{
				throw std::invalid_argument("同じGPU画像が既に登録されています");
			}
		}

		// 登録データを作成してstorage_に追加
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.resources.size());
		GraphResourceRecord resourceRecord{};
		resourceRecord.name = desc.name;
		resourceRecord.importedTexture = std::move(desc);
		storage_.resources.push_back(std::move(resourceRecord));

		// GraphResourceHandleを返す
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

		// 登録データを作成してstorage_に追加
		const std::uint32_t index = static_cast<std::uint32_t>(storage_.views.size());
		GraphViewRecord viewRecord{};
		viewRecord.desc = std::move(desc);
		storage_.views.push_back(std::move(viewRecord));

		// GraphViewHandleを返す
		return GraphViewHandle{ storage_.graphid, index };
	}

	// Passを登録する。GraphPassDescとGraphRecordFnを保持する。
	void RenderGraph::AddPass(GraphPassDesc desc, GraphRecordFn record)
	{
		// 前検査
		RequireBuilding();
		if (!record)
		{
			throw std::invalid_argument("GraphRecordFnがnullptrです");
		}
		validator_.ValidatePass(desc, storage_);

		// 登録データを作成してstorage_に追加
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
	// 入力を変更せず、Resolver結果をValidatorで再照合する。Resolve本体は未実装。
	void RenderGraph::Validate() const
	{
		const auto resolved = resolver_.Resolve(storage_);
		validator_.Validate(storage_, resolved);
	}

	// 全計画をローカル完成後に公開。失敗はBuilding/登録内容を保持する。
	void RenderGraph::Compile()
	{
		RequireBuilding();

		// ResolveとValidateを行い、コンパイル計画を作成する
		const auto resolved = resolver_.Resolve(storage_);
		validator_.Validate(storage_, resolved);

		const auto plan = compiler_.Compile(storage_, resolved);
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
	
		//frameから記録中のCommandContextとConstantBufferArenaを取得する
		auto& commandContext = frame.GetContext();
		auto& constantBufferArena = frame.GetConstants();

		//commandContextが記録中か
		if (!commandContext.GetRecordingList())
		{
			throw std::logic_error("CommandContextは記録中ではありません");
		}

		// 記録に使うCommandListを取得する
		auto* commandList = commandContext.GetRecordingList();
		state_ = State::Recording;
		try
		{
			// compiledPlan_->passesを順番にループ
			for (const auto& plannedPass : compiledPlan_->passes)
			{
				// 各パスのtransitionsを先に記録する
				for (const auto& transition : plannedPass.transitions)
				{
					//各transitionについて、画像を取り出し
					auto resource = storage_.resources[transition.resource.index].importedTexture->resource;

					//バリアを作って、commandListに記録する
					D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
						resource,
						transition.before,
						transition.after,
						D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
					);
					commandList->ResourceBarrier(1, &barrier);

					//そのパスのコールバックを呼ぶ
					GraphExecutionContext executionContext(*this, commandContext, constantBufferArena, plannedPass.passIndex);
					storage_.passes[plannedPass.passIndex].record(executionContext);
				}
			}

			//finalTransitionsを記録する
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

			// 記録が成功した場合、状態をRecordedに変更する
			state_ = State::Recorded;

		}
		catch (...)
		{
			commandContext.Invalidate();
			state_ = State::Failed;
			throw; // 例外を再スローして呼び出し元に伝える
		}
	}

}
