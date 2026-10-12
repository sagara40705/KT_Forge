#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Core/Math/Matrix4.h>
#include <filesystem>
#include <memory>
#include <span>
#include <cstdint>
#include <dxgiformat.h>

namespace KT::Graphics
{
	class GraphicsDevice;
	class CommandQueue;
	class CommandContext;
	class ColorTargetView;
	class DepthTargetView;
}

namespace KT::Renderer
{
	struct RenderView;

	// 上流のfloat行列分解でゼロ除算・overflowを起こさないaffine行列かを検査する。
	[[nodiscard]] bool IsValidEffectTransform(const KT::Core::Math::Matrix4& matrix) noexcept;

	// 一つのPlayerに属する素材。Effekseerのpointerや再生handleは公開しない。
	struct EffectAsset
	{
		std::uint64_t owner = 0, id = 0;
		bool operator==(const EffectAsset&) const = default;
	};

	// 完成Sceneからコピーした再生個体。World/Entityへの依存を持たない。
	struct EffectInstance
	{
		std::uint64_t id = 0;
		EffectAsset asset;
		KT::Core::Math::Matrix4 world;
	};

	// Effekseerの素材・再生・DX12描画資源を所有する。単一thread・同期1slot専用。
	// Device/Queueと、このPlayerを借りるFrameは外部で寿命を管理する。
	class EffectPlayer : private KT::Core::NonCopyable
	{
	public:
		EffectPlayer(KT::Graphics::GraphicsDevice& device, KT::Graphics::CommandQueue& queue, DXGI_FORMAT targetFormat);
		~EffectPlayer();
		// 起動時にefkefcと相対Texture/Modelを読み込む。失敗では既存素材を保持する。
		EffectAsset Load(const std::filesystem::path& path);
		[[nodiscard]] bool Contains(EffectAsset asset) const noexcept;
		// 成功したCPU tickの個体を反映し、秒を60fps基準の再生時間へ変換する。
		void Update(std::span<const EffectInstance> instances, double deltaSeconds);
		void StopAll();
		[[nodiscard]] bool IsPlaying(std::uint64_t id) const;
		[[nodiscard]] std::uint64_t OwnerId() const noexcept;
		[[nodiscard]] std::size_t PlayingCount() const;
		[[nodiscard]] int LastDrawCalls() const noexcept;
		// Graph記録前に、前回のGPU完了と上流poolの再利用を準備する。
		void PrepareFrame(KT::Graphics::CommandQueue& queue);
		// Graphが検査したRTVとreadonly DSVに記録する。遷移/送信/Presentを行わない。
		void Record(KT::Graphics::CommandContext& commands, const KT::Graphics::ColorTargetView& color,
			const KT::Graphics::DepthTargetView& depth, const RenderView& view);

	private:
		// 上流APIを一つの実装に隠し、ゲーム・Worldへincludeを伝播させない。
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
