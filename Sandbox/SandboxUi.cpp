#include <SandboxUi.h>
#include <Windows.h>
#include <array>
#include <stdexcept>

namespace KT::Sandbox
{
	SandboxUi::SandboxUi(KT::Application::Application& application)
		: application_(application),
		  ui_(application.UI())
	{
		// Sandbox素材はexe横に配置し、起動CWDから独立させる。
		std::array<wchar_t, 32768> filename{};
		const auto length = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
		if (length == 0 || length >= filename.size())
		{
			throw std::runtime_error("SandboxのUI素材rootとなるexeパスを取得できませんでした。");
		}
		const auto directory = std::filesystem::path(filename.data()).parent_path() / "ProjectAssets" / "UI";
		ui_.LoadFont(directory / "NotoSansCJKjp-Regular.otf");
		hud_ = ui_.LoadDocument(directory / "hud.rml");
		pause_ = ui_.LoadDocument(directory / "pause.rml");
		ui_.Show(hud_, true);
		const std::array keys{KT::Platform::Key::Escape};
		application_.SetFrameKeys(keys);
	}

	KT::RuntimeIntegration::UiDocument SandboxUi::Hud() const noexcept
	{
		return hud_;
	}

	void SandboxUi::Update()
	{
		// Escapeとボタンは時間停止中も解釈する。Engineには操作名の意味を持たせない。
		if (application_.FrameInput().keyboard.GetKey(KT::Platform::Key::Escape).pressed)
		{
			if (application_.GetExecutionState() == KT::Application::ExecutionState::Playing)
			{
				application_.Pause();
			}
			else
			{
				application_.Play();
			}
		}
		for (const auto& action : ui_.TakeActions())
		{
			if (action == "pause")
			{
				application_.Pause();
			}
			else if (action == "resume")
			{
				application_.Play();
			}
			else if (action == "quit")
			{
				application_.RequestStop();
			}
		}
		const bool paused = application_.GetExecutionState() != KT::Application::ExecutionState::Playing;
		if (paused != pauseVisible_)
		{
			ui_.Show(pause_, paused, paused);
			if (!paused)
			{
				ui_.Focus(hud_);
			}
			pauseVisible_ = paused;
		}
	}
}
