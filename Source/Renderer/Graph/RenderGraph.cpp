#include "Renderer/Graph/RenderGraph.h"
#include "Renderer/Graph/GraphExecutionContext.h"
#include "Graphics/CommandContext.h"
#include <cstddef>
#include <utility>

namespace KT::Renderer
{
	void RenderGraph::RequireBuilding() const
	{
		if (state_ != State::Building)
		{
			throw std::logic_error("GraphはBuilding状態ではありません。");
		}
	}

	// GraphResourceHandleの検査(IsValid・graphidの一致・indexの範囲)
	bool RenderGraph::Contains(GraphResourceHandle handle) const
	{
		if (!handle.IsValid()) return false;
		if (handle.graphid != graphid_) return false;
		if (handle.index >= resources_.size()) return false;
		return true;
	}

	// リソースを登録し、GraphResourceHandleを返す
	GraphResourceHandle RenderGraph::RegisterResource(std::string name)
	{
		// 前検査
		RequireBuilding();
		if (resources_.size() >= (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::runtime_error("リソースの登録数が最大に達しました。");
		}


		// 追加前のsizeを、新しいリソースのindexとして覚える
		std::uint32_t index = static_cast<std::uint32_t>(resources_.size());

		ResourceRecord record{};
		record.name = name;

		// リソースを登録
		resources_.push_back(std::move(record));

		// GraphResourceHandleを作成して返す
		GraphResourceHandle handle;
		handle.graphid = graphid_;
		handle.index = index;

		return handle;
	}

	// パスを追加する
	void RenderGraph::AddPass(GraphPassDesc desc, GraphRecordFn record)
	{
		// 前検査
		RequireBuilding();
		if (desc.name.empty())
		{
			throw std::invalid_argument("パス名が空です。");
		}
		if (!record)
		{
			throw std::invalid_argument("パスに登録された記録関数が空です。");
		}

		// 同じリソースの重複指定を拒否する
		std::vector<bool> checkedIndex(resources_.size(), false);

		for (const auto& use : desc.resources)
		{
			if (!Contains(use.resource))
			{
				throw std::invalid_argument("パスに登録されたリソースが無効です。");
			}
			if (use.access != GraphResourceAccess::Read &&
				use.access != GraphResourceAccess::WriteAll &&
				use.access != GraphResourceAccess::ReadWrite)
			{
				throw std::invalid_argument("パスに登録されたリソースのアクセス種別が無効です。");
			}
			if (use.usage != GraphResourceUsage::Unspecified &&
				use.usage != GraphResourceUsage::RenderTarget)
			{
				throw std::invalid_argument("パスに登録されたリソースの用途が無効です。");
			}

			// 重複チェック
			const auto index = use.resource.index;
			if (checkedIndex[index])
			{
				throw std::invalid_argument("パスに登録されたリソースが重複しています。");
			}

			checkedIndex[index] = true;
		}

		// PassRecordを作成してpasses_に追加する
		PassRecord recordEntry{};
		recordEntry.desc = std::move(desc);
		recordEntry.record = std::move(record);
		passes_.push_back(std::move(recordEntry));
	}

	// Graphの検査
	void RenderGraph::Validate() const
	{
		//登録リソース数と同じ長さの「内容が定義済みか」の配列を、全部falseで作る
		const auto resourceCount = resources_.size();
		std::vector<bool> resourceDefined(resourceCount, false);

		//resources_のうち、importedTextureがあるものはcontentsDefinedをresourceDefinedにコピーする
		for (std::size_t index = 0; index < resources_.size(); ++index)
		{
			if (resources_[index].importedTexture.has_value())
			{
				resourceDefined[index] = resources_[index].importedTexture->contentsDefined;
			}
		}

		//passes_を登録順に調べる

		//各パスのresourcesを調べ、
		//ReadかReadWriteなのに対応する値がfalseならstd::runtime_errorをthrowする
		//WriteAllかReadWriteなら対応する値をtrueにする
		for (const auto& pass : passes_)
		{
			const auto& desc = pass.desc;

			for (const auto& use : desc.resources)
			{
				const auto index = use.resource.index;
				if (index >= resourceCount)
				{
					throw std::runtime_error("パスに登録されたリソースのインデックスが無効です。");
				}
				if (use.access == GraphResourceAccess::Read || use.access == GraphResourceAccess::ReadWrite)
				{
					if (!resourceDefined[index])
					{
						throw std::runtime_error("パス '" + desc.name + "' で読み込まれるリソース '" + resources_[index].name + "' が未定義です。");
					}
				}
				if (use.access == GraphResourceAccess::WriteAll || use.access == GraphResourceAccess::ReadWrite)
				{
					resourceDefined[index] = true;
				}
			}
		}

		// 
		for (std::size_t index = 0; index < resources_.size(); ++index)
		{
			if (!resources_[index].importedTexture.has_value())
			{
				continue;
			}
			if (resources_[index].importedTexture->requireDefineAtEnd && !resourceDefined[index])
			{
				throw std::runtime_error("リソース '" + resources_[index].name + "' はGraph終了時に必要な画像内容が定義されていません。");
			}
		}
	}

	GraphResourceHandle RenderGraph::ImportTexture(GraphImportedTextureDesc desc)
	{
		// 前検査
		RequireBuilding();
		if (desc.name.empty())
		{
			throw std::invalid_argument("インポートするテクスチャの名前が空です。");
		}
		if (!desc.resource)
		{
			throw std::invalid_argument("インポートするテクスチャのリソースがnullptrです。");
		}
		if (desc.kind == GraphTextureKind::Color && desc.rtv.ptr == 0)
		{
			throw std::invalid_argument("インポートするテクスチャのRTVが無効です。");
		}
		if (desc.kind == GraphTextureKind::Depth && desc.dsv.ptr == 0)
		{
			throw std::invalid_argument("インポートするテクスチャのDSVが無効です。");
		}

		if (resources_.size() >= ((std::numeric_limits<std::uint32_t>::max)()))
		{
			throw std::overflow_error("リソースの登録数が最大です。");
		}

		//　同じ画像の二重登録を拒否する
		for (const auto& record : resources_)
		{
			if (record.importedTexture.has_value() && record.importedTexture->resource == desc.resource)
			{
				throw std::invalid_argument("同じ画像の二重登録はできません。");
			}
		}

		//追加前のresources_.size()をuint32_tへ変換し、indexとして保存する
		const auto index = static_cast<std::uint32_t>(resources_.size());

		// ResourceRecordを作成してresources_に追加する
		ResourceRecord record{};
		record.name = desc.name;
		record.importedTexture = std::move(desc);
		resources_.push_back(std::move(record));

		// GraphResourceHandleを作成して返す
		GraphResourceHandle handle;
		handle.graphid = graphid_;
		handle.index = index;

		return handle;
	}

	// Graphの計画を作成する
	void RenderGraph::Compile()
	{
		// 前検査
		RequireBuilding();
		Validate();

		// resources_を走査する
		for (const auto& record : resources_)
		{
			if (!record.importedTexture.has_value())
			{
				throw std::runtime_error("インポートされていないリソースがあります。");
			}

			const auto& imported = record.importedTexture;
			const auto imageDesc = imported->resource->GetDesc();

			// 2Dテクスチャであることを検査する
			if (imageDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' は2Dテクスチャではありません。");
			}
			if (imageDesc.Width == 0 || imageDesc.Height == 0)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' の幅または高さが0です。");
			}
			if (imageDesc.DepthOrArraySize != 1)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' は2Dテクスチャではありません。");
			}
			if (imageDesc.MipLevels != 1)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' のミップレベルが1ではありません。初版は1のみ");
			}
			if (imageDesc.SampleDesc.Count != 1 || imageDesc.SampleDesc.Quality != 0)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' はマルチサンプルテクスチャです。");
			}
			if ((imageDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
			{
				throw std::runtime_error("インポートされたリソース '" + record.name + "' はレンダーターゲットとして使えません。");
			}

			// initialStateの検査
			if (imported->initialState != D3D12_RESOURCE_STATE_RENDER_TARGET &&
				imported->initialState != D3D12_RESOURCE_STATE_COMMON)
			{
				throw std::runtime_error("初版のGraphでは対応していないinitialStateです。");
			}
			// finalStateの検査
			if (imported->finalState != D3D12_RESOURCE_STATE_RENDER_TARGET &&
				imported->finalState != D3D12_RESOURCE_STATE_COMMON)
			{
				throw std::runtime_error("初版のGraphでは対応していないfinalStateです。");
			}
		}

		// passes_を走査する
		for (const auto& pass : passes_)
		{
			if (!pass.record)
			{
				throw std::runtime_error("パス '" + pass.desc.name + "' のレコードが空です。");
			}
			for (const auto& use : pass.desc.resources)
			{
				if (!Contains(use.resource))
				{
					throw std::runtime_error("パス '" + pass.desc.name + "' のリソースHandleが無効です。");
				}
				if (use.usage != GraphResourceUsage::RenderTarget)
				{
					throw std::runtime_error("パス '" + pass.desc.name + "' のリソース '" + resources_[use.resource.index].name + "' の用途が無効です。");
				}
				if (use.access != GraphResourceAccess::WriteAll &&
					use.access != GraphResourceAccess::ReadWrite)
				{
					throw std::runtime_error("パス '" + pass.desc.name + "' のリソース '" + resources_[use.resource.index].name + "' のアクセス種別が無効です。");
				}
			}
		}

		// 一時的な計画	
		CompiledPlan plan{};
		std::vector<D3D12_RESOURCE_STATES> currentStates;
		currentStates.reserve(resources_.size());

		// 計画上の現在状態
		for (const auto& record : resources_)
		{
			currentStates.push_back(record.importedTexture->initialState);
		}

		// passes_を登録順に走査する
		for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			const auto& pass = passes_[passIndex];
			PlannedPass plannedPass;
			plannedPass.passIndex = passIndex;

			// そのパスが使うリソースを走査する
			for (const auto& use : pass.desc.resources)
			{
				const auto resourceIndex = use.resource.index;
				const auto before = currentStates[resourceIndex];
				const auto required = D3D12_RESOURCE_STATE_RENDER_TARGET; // 今回はRenderTargetのみ対応

				// 状態が異なる場合だけ、遷移を追加する
				if (before != required)
				{
					PlannedTransition transition;
					transition.resource = use.resource;
					transition.before = before;
					transition.after = required;
					plannedPass.transitions.push_back(std::move(transition));
					// 計画上の現在状態を更新する
					currentStates[resourceIndex] = required;
				}
			}

			// パスの計画を保存する
			plan.passes.push_back(std::move(plannedPass));
		}

		// 画像をfinalStateへ戻すための遷移を計画する
		for (std::size_t resourceIndex = 0; resourceIndex < resources_.size(); ++resourceIndex)
		{
			const auto before = currentStates[resourceIndex];
			const auto after = resources_[resourceIndex].importedTexture->finalState;
			if (before != after)
			{
				PlannedTransition transition;
				transition.resource.graphid = graphid_;
				transition.resource.index = static_cast<std::uint32_t>(resourceIndex);
				transition.before = before;
				transition.after = after;
				plan.finalTransitions.push_back(std::move(transition));
			}
			
		}

		compiledPlan_ = std::move(plan);
		state_ = State::Completed;
	}

	// Graphの計画に従い、記録中のCommandContextへ命令を記録する
	void RenderGraph::Record(KT::Graphics::CommandContext& commandContext)
	{
		// 前検査
		if (state_ != State::Completed)
		{
			throw std::logic_error("GraphはCompleted状態ではありません。");
		}
		if (!compiledPlan_.has_value())
		{
			throw std::logic_error("Graphの計画が作成されていません。");
		}

		// CommandListが記録中であることを確認する
		(void)commandContext.GetRecordingList();

		try
		{
			// 記録中の状態にする
			state_ = State::Recording;

			// 計画に従い、各パスの命令を記録する
			for (const auto& plannedPass : compiledPlan_->passes)
			{
				// 各Transitionについて、Import情報を取得する
				// CommandContextへ記録を依頼する
				for (const auto& transition : plannedPass.transitions)
				{
					const auto& imported = resources_[transition.resource.index].importedTexture;
					commandContext.Transition(imported->resource, transition.before, transition.after);
				}

				// パスごとにGraphExecutionContextを作成し、パスのrecord関数を呼び出す
				GraphExecutionContext context(*this, commandContext, plannedPass.passIndex);
				passes_[plannedPass.passIndex].record(context);
			}

			// 最終遷移についても同様にCommandContextへ記録を依頼する
			for (const auto& transition : compiledPlan_->finalTransitions)
			{
				const auto& imported = resources_[transition.resource.index].importedTexture;
				commandContext.Transition(imported->resource, transition.before, transition.after);
			}

			// 記録が完了したので、状態をRecordedにする
			state_ = State::Recorded;
		}
		catch (...)
		{
			commandContext.Invalidate();
			state_ = State::Failed;
			throw;
		}
	}
}




