#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Platform/Input/InputSnapshot.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <span>

namespace KT::Renderer
{
	class UiRenderer;
}

namespace KT::RuntimeIntegration
{
	// 一つのUI窓口に属するdocument。閉じた番号は再利用しない。
	struct UiDocument
	{
		std::uint64_t owner = 0, id = 0;
		bool operator==(const UiDocument&) const = default;
	};

	// ゲームの値をRML画面へ反映し、ボタンの値イベントをゲームへ返す。
	// Rendererを借用する。World/Component参照と上流DOMをゲームへ公開しない。
	class GameUi : private KT::Core::NonCopyable
	{
	public:
		explicit GameUi(KT::Renderer::UiRenderer& renderer);
		~GameUi();
		void LoadFont(const std::filesystem::path& path);
		UiDocument LoadDocument(const std::filesystem::path& path);
		void Close(UiDocument document);
		void Show(UiDocument document, bool visible, bool modal = false);
		// アプリが選ぶ戻り先へfocusを移す。閉じたmodalのbuttonへfocusを残さない。
		void Focus(UiDocument document);
		// UTF-8のplain text。RMLとして解釈せず、markup文字をescapeする。
		void SetText(UiDocument document, const std::string& elementId, const std::string& text);
		void SetImage(UiDocument document, const std::string& elementId, const std::filesystem::path& path);
		// width百分率を変更する。currentは0..maximum、maximumは正の有限値。
		void SetHealth(UiDocument document, const std::string& barId, float current, float maximum);
		// data-game-actionのclickを値として返す。意味はアプリが決める。
		std::vector<std::string> TakeActions();
		// 予約キーは外側loopだけへ配送し、文字編集focus中はUIへ渡す。
		void SetFrameKeys(std::span<const KT::Platform::Key> keys);
		[[nodiscard]] const KT::Platform::InputSnapshot& FrameInput() const noexcept;
		// ApplicationのUI更新。FramebufferとContent寸法を分け、最小化中も入力を扱う。
		void Update(const KT::Platform::InputSnapshot& raw, int windowWidth, int windowHeight, unsigned width, unsigned height);
		[[nodiscard]] const KT::Platform::InputSnapshot& GameInput() const noexcept;
		void ConsumeGameInput() noexcept;
		// Texture転送・描画列の捕捉をGraph/Frame開始前に完了する。
		void PrepareFrame(unsigned width, unsigned height);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
