#include <Renderer/Graph/RenderGraph.h>
#include <cstdint>
#include <stdexcept>
#include <limits>


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
	GraphResourceHandle RenderGraph::ImportTexture(GraphImportedTextureDesc desc)
	{
		// 前検査
		RequireBuilding();
		return GraphResourceHandle();
	}
	GraphViewHandle RenderGraph::AddView(GraphViewDesc desc)
	{
		// 前検査
		RequireBuilding();
		return GraphViewHandle();
	}
	void RenderGraph::AddPass(GraphPassDesc desc, GraphRecordFn record)
	{
		// 前検査
		RequireBuilding();
	}
	// 登録できる状態かを確認する。Building以外は登録不可。
	void RenderGraph::RequireBuilding() const
	{
		if (state_ != State::Building)
		{
			throw std::logic_error("GraphはBuilding状態ではありません。登録操作は許可されません。");
		}
	}
}
