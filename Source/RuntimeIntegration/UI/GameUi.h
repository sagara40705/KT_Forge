#pragma once
#include <Core/Utility/NonCopyable.h>
#include <Platform/Input/InputSnapshot.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

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
		// UTF-8のplain text。RMLとして解釈せず、markup文字をescapeする。
		void SetText(UiDocument document, const std::string& elementId, const std::string& text);
		void SetImage(UiDocument document, const std::string& elementId, const std::filesystem::path& path);
		// width百分率を変更する。currentは0..maximum、maximumは正の有限値。
		void SetHealth(UiDocument document, const std::string& barId, float current, float maximum);
		// 標準HUD/ポーズを読み込む。各画面はRML/RCSS側で編集できる。
		void LoadBuiltin(const std::filesystem::path& directory);
		[[nodiscard]] UiDocument HudDocument() const noexcept;
		[[nodiscard]] UiDocument PauseDocument() const noexcept;
		void SetPaused(bool paused);
		[[nodiscard]] bool IsPaused() const noexcept;
		// data-game-actionのclickを値で返す。pause/resumeは同時に内部へ反映する。
		std::vector<std::string> TakeActions();
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
