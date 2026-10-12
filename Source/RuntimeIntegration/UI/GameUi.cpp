#include <RuntimeIntegration/UI/GameUi.h>
#include <RuntimeIntegration/UI/UiInput.h>
#include <Renderer/UI/UiRenderer.h>
#include <Core/Log.h>
#include <RmlUi/Core.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <stdexcept>
#include <Windows.h>

namespace KT::RuntimeIntegration
{
	namespace
	{
		// 上流global interfaceの所有は、一つのGameUiだけへ限定する。
		bool activeUi = false;
		std::uint64_t nextOwner = 1;

		std::string Utf8Path(const std::filesystem::path& path)
		{
			const auto value = path.generic_u8string();
			return {reinterpret_cast<const char*>(value.data()), value.size()};
		}

		void ValidateText(const std::string& text)
		{
			if (text.size() > 65536 || text.find('\0') != std::string::npos ||
				(!text.empty() &&
					MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0) == 0))
			{
				throw std::invalid_argument("UI文字列はNULなしの有効なUTF-8、65536 byte以下にしてください。");
			}
		}

		// Unicodeの制作パスをCRTのwide APIで読み、RmlUiのfile interfaceへ接続する。
		class UiFiles : public Rml::FileInterface
		{
		public:
			Rml::FileHandle Open(const Rml::String& path) override
			{
				const auto filename = std::filesystem::u8path(path);
				FILE* file = nullptr;
				_wfopen_s(&file, filename.c_str(), L"rb");
				return reinterpret_cast<Rml::FileHandle>(file);
			}

			void Close(Rml::FileHandle file) override
			{
				std::fclose(reinterpret_cast<FILE*>(file));
			}

			std::size_t Read(void* data, std::size_t bytes, Rml::FileHandle file) override
			{
				return std::fread(data, 1, bytes, reinterpret_cast<FILE*>(file));
			}

			bool Seek(Rml::FileHandle file, long offset, int origin) override
			{
				return _fseeki64(reinterpret_cast<FILE*>(file), offset, origin) == 0;
			}

			std::size_t Tell(Rml::FileHandle file) override
			{
				const auto offset = _ftelli64(reinterpret_cast<FILE*>(file));
				if (offset < 0)
				{
					throw std::runtime_error("UIファイルの現在位置を取得できませんでした。");
				}
				return static_cast<std::size_t>(offset);
			}
		};

		// Scene時計と独立した経過時間と、既存Logへの上流診断接続を担当する。
		class UiSystem : public Rml::SystemInterface
		{
		public:
			double GetElapsedTime() override
			{
				return std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
			}

			bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
			{
				const auto level = type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT ? KT::Core::LogLevel::Error :
					type == Rml::Log::LT_WARNING											 ? KT::Core::LogLevel::Warning :
																							   KT::Core::LogLevel::Info;
				(void)KT::Core::Log(level, "RmlUi診断: " + message);
				return true;
			}

		private:
			// Sceneの固定時計と独立したUI animation用の起動時刻。
			std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
		};
	}

	// Contextとdocument状態を所有し、clickをゲーム向けの値へ変換する。
	struct GameUi::Impl : Rml::EventListener
	{
		// 接続先はこのContext/上流globalより長く保持する。
		KT::Renderer::UiRenderer& renderer;
		UiFiles files;
		UiSystem system;
		Rml::Context* context = nullptr;
		UiInput input;
		// 窓口識別とdocument発行番号。0は公開handleの無効値。
		std::uint64_t owner = 0, nextDocument = 1;

		// DOM借用と入力遮断用の表示契約を、同じhandleに対応付ける。
		struct Document
		{
			// Contextが所有するDOM。Close/Shutdownまでだけ借用する。
			Rml::ElementDocument* value;
			bool visible = false, modal = false;
		};

		std::unordered_map<std::uint64_t, Document> documents;
		// ゲームへ渡すclick操作名。TakeActionsで所有を移す。
		std::vector<std::string> actions;

		explicit Impl(KT::Renderer::UiRenderer& graphics)
			: renderer(graphics)
		{
			if (activeUi || nextOwner == UINT64_MAX)
			{
				throw std::logic_error("初版RmlUiは同時に一つのGameUiだけを保持できます。");
			}
			owner = nextOwner++;
			Rml::SetFileInterface(&files);
			Rml::SetSystemInterface(&system);
			Rml::SetRenderInterface(&renderer.Interface());
			try
			{
				if (!Rml::Initialise())
				{
					throw std::runtime_error("RmlUiの初期化に失敗しました。");
				}
				context = Rml::CreateContext("KT_Forge", {1, 1});
				if (!context)
				{
					throw std::runtime_error("RmlUiのContextを生成できませんでした。");
				}
				context->AddEventListener("click", this);
				activeUi = true;
			}
			catch (...)
			{
				Rml::Shutdown();
				Rml::SetFileInterface(nullptr);
				Rml::SetSystemInterface(nullptr);
				Rml::SetRenderInterface(nullptr);
				throw;
			}
		}

		~Impl()
		{
			context->RemoveEventListener("click", this);
			Rml::Shutdown();
			Rml::SetFileInterface(nullptr);
			Rml::SetSystemInterface(nullptr);
			Rml::SetRenderInterface(nullptr);
			activeUi = false;
		}

		Document& Require(UiDocument document)
		{
			if (document.owner != owner || !documents.contains(document.id))
			{
				throw std::invalid_argument("UI documentは別の窓口に属するか、既に閉じています。");
			}
			return documents.at(document.id);
		}

		Rml::Element& Element(UiDocument document, const std::string& id)
		{
			ValidateText(id);
			auto* element = Require(document).value->GetElementById(id);
			if (!element)
			{
				throw std::invalid_argument("UI documentに指定element IDがありません: " + id);
			}
			return *element;
		}

		void ProcessEvent(Rml::Event& event) override
		{
			// DOMをゲームへ渡さず、button祖先の操作名を値として保持する。
			for (auto* element = event.GetTargetElement(); element && element != context->GetRootElement();
				element = element->GetParentNode())
			{
				const auto action = element->GetAttribute<Rml::String>("data-game-action", "");
				if (!action.empty())
				{
					if (actions.size() >= 256)
					{
						throw std::overflow_error("UI操作イベントが上限256に達しました。ゲーム側でTakeActionsしてください。");
					}
					actions.push_back(action);
					break;
				}
			}
		}
	};

	GameUi::GameUi(KT::Renderer::UiRenderer& renderer)
		: impl_(std::make_unique<Impl>(renderer))
	{
	}

	GameUi::~GameUi() = default;

	void GameUi::LoadFont(const std::filesystem::path& path)
	{
		if (!std::filesystem::is_regular_file(path) || !Rml::LoadFontFace(Utf8Path(std::filesystem::absolute(path)), true))
		{
			throw std::runtime_error("UIフォントを読み込めませんでした: " + Utf8Path(path));
		}
	}

	UiDocument GameUi::LoadDocument(const std::filesystem::path& path)
	{
		if (impl_->documents.size() >= 32 || impl_->nextDocument == UINT64_MAX)
		{
			throw std::overflow_error("UI document数または番号が上限に達しました。");
		}
		auto* document = impl_->context->LoadDocument(Utf8Path(std::filesystem::absolute(path)));
		if (!document)
		{
			throw std::runtime_error("UI documentを読み込めませんでした: " + Utf8Path(path));
		}
		const auto id = impl_->nextDocument++;
		try
		{
			impl_->documents.emplace(id, Impl::Document{document});
		}
		catch (...)
		{
			document->Close();
			throw;
		}
		return {impl_->owner, id};
	}

	void GameUi::Close(UiDocument document)
	{
		impl_->Require(document).value->Close();
		impl_->documents.erase(document.id);
	}

	void GameUi::Show(UiDocument handle, bool visible, bool modal)
	{
		auto& document = impl_->Require(handle);
		if (visible)
		{
			document.value->Show(modal ? Rml::ModalFlag::Modal : Rml::ModalFlag::None);
		}
		else
		{
			document.value->Hide();
		}
		document.visible = visible;
		document.modal = visible && modal;
	}

	void GameUi::Focus(UiDocument document)
	{
		(void)impl_->Require(document).value->Focus();
	}

	void GameUi::SetText(UiDocument document, const std::string& id, const std::string& text)
	{
		ValidateText(text);
		std::string escaped;
		for (const auto character : text)
		{
			switch (character)
			{
			case '&':
				escaped += "&amp;";
				break;
			case '<':
				escaped += "&lt;";
				break;
			case '>':
				escaped += "&gt;";
				break;
			default:
				escaped += character;
				break;
			}
		}
		impl_->Element(document, id).SetInnerRML(escaped);
	}

	void GameUi::SetImage(UiDocument document, const std::string& id, const std::filesystem::path& path)
	{
		if (!std::filesystem::is_regular_file(path))
		{
			throw std::invalid_argument("UI画像ファイルが存在しません: " + Utf8Path(path));
		}
		auto& element = impl_->Element(document, id);
		if (element.GetTagName() != "img")
		{
			throw std::invalid_argument("UI画像の変更先はimg要素にしてください。");
		}
		element.SetAttribute("src", Utf8Path(std::filesystem::absolute(path)));
	}

	void GameUi::SetHealth(UiDocument document, const std::string& id, float current, float maximum)
	{
		if (!std::isfinite(current) || !std::isfinite(maximum) || maximum <= 0 || current < 0 || current > maximum)
		{
			throw std::invalid_argument("UIのHPは有限値で0..最大HP、最大HPは正にしてください。");
		}
		impl_->Element(document, id).SetProperty("width", std::to_string(static_cast<double>(current) / maximum * 100) + "%");
	}

	std::vector<std::string> GameUi::TakeActions()
	{
		std::vector<std::string> result;
		result.swap(impl_->actions);
		return result;
	}

	void GameUi::Update(const KT::Platform::InputSnapshot& raw, int windowWidth, int windowHeight, unsigned width, unsigned height)
	{
		if (width > 16384 || height > 16384)
		{
			throw std::invalid_argument("UIのFramebuffer寸法は16384以下にしてください。");
		}
		if (width != 0 && height != 0)
		{
			impl_->context->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
			if (windowWidth > 0)
			{
				impl_->context->SetDensityIndependentPixelRatio(static_cast<float>(width) / windowWidth);
			}
		}
		const bool wasModal = std::any_of(
			impl_->documents.begin(), impl_->documents.end(), [](const auto& pair) { return pair.second.visible && pair.second.modal; });
		impl_->context->Update();
		impl_->input.Route(impl_->context, raw, windowWidth, windowHeight, wasModal);
		// clickで表示を変えた後のlayoutも、この描画前に完了する。
		impl_->context->Update();
	}

	void GameUi::SetFrameKeys(std::span<const KT::Platform::Key> keys)
	{
		impl_->input.SetFrameKeys(keys);
	}

	const KT::Platform::InputSnapshot& GameUi::FrameInput() const noexcept
	{
		return impl_->input.FrameInput();
	}

	const KT::Platform::InputSnapshot& GameUi::GameInput() const noexcept
	{
		return impl_->input.GameInput();
	}

	void GameUi::ConsumeGameInput() noexcept
	{
		impl_->input.Consume();
	}

	void GameUi::PrepareFrame(unsigned width, unsigned height)
	{
		// 上流のint寸法へ縮小する前に、描画可能な範囲を検査する。
		if (width == 0 || height == 0 || width > 16384 || height > 16384)
		{
			throw std::invalid_argument("UI描画のFramebuffer寸法は1..16384にしてください。");
		}
		impl_->context->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
		impl_->context->Update();
		impl_->renderer.BeginCapture(width, height);
		try
		{
			impl_->context->Render();
			impl_->renderer.EndCapture();
		}
		catch (...)
		{
			impl_->renderer.AbortCapture();
			throw;
		}
	}
}
