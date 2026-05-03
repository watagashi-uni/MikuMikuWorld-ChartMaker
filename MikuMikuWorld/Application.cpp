#include "Application.h"
#include "ResourceManager.h"
#include "IO.h"
#include "Utilities.h"
#include "Localization.h"
#include "ApplicationConfiguration.h"
#include "ScoreSerializer.h"
#include "NoteSkin.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace MikuMikuWorld
{
#ifndef MIKUMIKUWORLD_VERSION
#define MIKUMIKUWORLD_VERSION "1.0.0"
#endif

	std::string Application::version{ "1.0.0" };
	std::string Application::appDir{ "" };
	static std::string userDataDir{ "" };
	std::string Application::pendingLoadScoreFile{ "" };
	WindowState Application::windowState{};

	namespace
	{
		std::string ensureTrailingSlash(std::string path)
		{
			if (!path.empty() && path.back() != '/')
				path.push_back('/');

			return path;
		}

		std::string resolveUserDataDir(const std::string& resourceDir)
		{
#ifdef __APPLE__
			const char* homeDir = std::getenv("HOME");
			if (homeDir && *homeDir)
				return ensureTrailingSlash(std::string(homeDir) + "/Library/Application Support/MikuMikuWorld");
#endif
			return resourceDir;
		}

		void ensureDirectoryExists(const std::string& path)
		{
			if (path.empty())
				return;

			std::error_code error;
			std::filesystem::create_directories(path, error);
		}
	}

	Application::Application() : 
		initialized{ false }, language{ "" }
	{
	}

	Result Application::initialize(const std::string& root)
	{
		if (initialized)
			return Result(ResultStatus::Success, "App is already initialized");

		appDir = ensureTrailingSlash(root);
		userDataDir = resolveUserDataDir(appDir);
		ensureDirectoryExists(userDataDir);
		version = getVersion();
		language = "";

		const std::string userConfigPath = userDataDir + APP_CONFIG_FILENAME;
		const std::string bundledConfigPath = appDir + APP_CONFIG_FILENAME;
		if (IO::File::exists(userConfigPath))
			config.read(userConfigPath);
		else
			config.read(bundledConfigPath);
		readSettings();

		Result result = initOpenGL();
		if (!result.isOk())
			return result;

		imgui = std::make_unique<ImGuiManager>();
		result = imgui->initialize(window);
		if (!result.isOk())
			return result;

		imgui->setBaseTheme(config.baseTheme);
		imgui->applyAccentColor(config.accentColor);

		loadResources();
		startupWarningShouldOpen = !config.hideStartupWarning && startupWarningTextureIndex >= 0;

		editor = std::make_unique<ScoreEditor>();
		editor->loadPresets();

		initialized = true;
		return Result::Ok();
	}

	const std::string& Application::getAppDir()
	{
		return appDir;
	}

	const std::string& Application::getUserDataDir()
	{
		return userDataDir;
	}

	std::string Application::getVersion()
	{
#ifdef _WIN32
		wchar_t filename[1024];
		lstrcpyW(filename, IO::mbToWideStr(std::string(appDir + "MikuMikuWorld.exe")).c_str());

		DWORD  verHandle = 0;
		UINT   size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD  verSize = GetFileVersionInfoSizeW(filename, &verHandle);

		int major = 0, minor = 0, build = 0, rev = 0;
		if (verSize != NULL)
		{
			LPSTR verData = new char[verSize];

			if (GetFileVersionInfoW(filename, verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, TEXT("\\"), (VOID FAR * FAR*) & lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							major = (verInfo->dwFileVersionMS >> 16) & 0xffff;
							minor = (verInfo->dwFileVersionMS >> 0) & 0xffff;
							rev = (verInfo->dwFileVersionLS >> 16) & 0xffff;
						}
					}
				}
			}
			delete[] verData;
		}

		return IO::formatString("%d.%d.%d", major, minor, rev);
#else
		return MIKUMIKUWORLD_VERSION;
#endif
	}

	const std::string& Application::getAppVersion()
	{
		return version;
	}

	void Application::dispose()
	{
		if (initialized)
		{
			editor->uninitialize();
			imgui->shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		initialized = false;
	}

	void Application::readSettings()
	{
		windowState.position = config.windowPos;
		windowState.size = config.windowSize;
		windowState.maximized = config.maximized;
		windowState.vsync = config.vsync;
		windowState.fullScreen = config.fullScreen;
		UI::accentColors[0] = config.userColor.toImVec4();
	}

	void Application::writeSettings()
	{
		config.maximized = windowState.maximized;
		config.vsync = windowState.vsync;
		config.windowPos = windowState.position;
		config.windowSize = windowState.size;
		config.fullScreen = windowState.fullScreen;
		config.userColor = Color::fromImVec4(UI::accentColors[0]);

		if (editor)
		{
			editor->writeSettings();
			config.write(userDataDir + APP_CONFIG_FILENAME);
		}
	}

	void Application::appendOpenFile(const std::string& filename)
	{
		pendingOpenFiles.push_back(filename);
		windowState.dragDropHandled = false;
	}

	void Application::handlePendingOpenFiles()
	{
		std::string scoreFile{};
		std::string musicFile{};

		for (auto it = pendingOpenFiles.rbegin(); it != pendingOpenFiles.rend(); ++it)
		{
			std::string extension = IO::File::getFileExtension(*it);
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

			if (ScoreSerializeController::isValidFormat(ScoreSerializeController::toSerializeFormat(*it)))
				scoreFile = *it;
			else if (Audio::isSupportedFileFormat(extension))
				musicFile = *it;

			if (!scoreFile.empty() && !musicFile.empty())
				break;
		}

		if (!scoreFile.empty())
		{
			windowState.resetting = true;
			pendingLoadScoreFile = scoreFile;
		}

		if (!musicFile.empty())
			editor->asyncLoadMusic(musicFile);

		pendingOpenFiles.clear();
		windowState.dragDropHandled = true;
	}

	void Application::update()
	{
		if (config.language != language)
		{
			std::string locale = config.language == "auto" ? Utilities::getSystemLocale() : config.language;
			
			// Try to set the selected language and fallback to default (en) on failure
			if (!Localization::setLanguage(locale))
				Localization::setLanguage("en");

			language = config.language;
		}

		float dpiX = 1.0f, dpiY = 1.0f;
#ifdef __APPLE__
		// GLFW reports Retina backing scale here, but the current UI code already
		// renders in logical points. Applying that scale again makes the macOS UI oversized.
		float dpiScale = 1.0f;
#else
		GLFWmonitor* mainMonitor = glfwGetPrimaryMonitor();
		if (mainMonitor)
		{
			glfwGetMonitorContentScale(mainMonitor, &dpiX, &dpiY);
		}

		float dpiScale = (dpiX + dpiY) * 0.5f;
#endif
		if (dpiScale != windowState.lastDpiScale)
		{
			imgui->buildFonts(dpiScale);
			windowState.lastDpiScale = dpiScale;
		}

		imgui->begin();

		// Inform ImGui of dpi changes
		ImGui::GetMainViewport()->DpiScale = dpiX;
		UI::updateBtnSizesDpiScaling(dpiScale);

		if (!windowState.dragDropHandled)
			handlePendingOpenFiles();

		imgui->initializeLayout();

		if (config.accentColor != imgui->getAccentColor())
			imgui->applyAccentColor(config.accentColor);

		if (config.userColor != Color::fromImVec4(UI::accentColors[0]) && config.accentColor == 0)
			imgui->applyAccentColor(config.accentColor);

		if (config.baseTheme != imgui->getBaseTheme())
			imgui->setBaseTheme(config.baseTheme);

		if ((windowState.closing || windowState.resetting) && !editor->isUpToDate() && !unsavedChangesDialog.open)
		{
			unsavedChangesDialog.open = true;
			ImGui::OpenPopup(MODAL_TITLE("unsaved_changes"));
		}

		auto unsavedChangesResult = unsavedChangesDialog.update();

		if (windowState.closing)
		{
			if (!editor->isUpToDate())
			{
				switch (unsavedChangesResult)
				{
				case DialogResult::Yes:
					editor->trySave(editor->getWorkingFilename().data());
					glfwSetWindowShouldClose(window, 1);
					break;

				case DialogResult::No:
					glfwSetWindowShouldClose(window, 1);
					break;

				case DialogResult::Cancel:
					windowState.closing = false;
					break;

				default:
					break;
				}
			}
			else
			{
				glfwSetWindowShouldClose(window, 1);
			}
		}

		if (windowState.resetting)
		{
			if (!editor->isUpToDate())
			{
				switch (unsavedChangesResult)
				{
				case DialogResult::Yes:
					editor->trySave(editor->getWorkingFilename().data());
					break;

				case DialogResult::Cancel:
					windowState.resetting = shouldPickScore = false;
					pendingLoadScoreFile.clear();
					break;

				default:
					break;
				}
			}

			// Already saved or clicked save changes or discard changes
			if (editor->isUpToDate() || (unsavedChangesResult != DialogResult::Cancel && unsavedChangesResult != DialogResult::None))
			{
				if (windowState.shouldPickScore)
				{
					editor->open();
					windowState.shouldPickScore = false;
				}
				else if (pendingLoadScoreFile.size())
				{
					editor->loadScore(pendingLoadScoreFile);
					pendingLoadScoreFile.clear();
				}
				else
				{
					editor->reset();
				}

				windowState.resetting = false;
			}
		}

		editor->update();

		bool isFullScreen = config.fullScreen;
		if (ImGui::IsAnyPressed(config.input.toggleFullscreen)) setFullScreen(!isFullScreen);

		if (!editor->isFullScreenPreview())
		{
			ImGui::BeginMainMenuBar();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 2));

			if (ImGui::BeginMenu(getString("window")))
			{
				if (ImGui::MenuItem(getString("fullscreen"), ToShortcutString(config.input.toggleFullscreen), &isFullScreen))
					setFullScreen(isFullScreen);

				ImGui::EndMenu();
			}

			ImGui::PopStyleVar();
			ImGui::EndMainMenuBar();
		}

		updateStartupWarningDialog();

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		imgui->draw(window);
		glfwSwapBuffers(window);
	}

	void Application::updateStartupWarningDialog()
	{
		if (config.hideStartupWarning || startupWarningTextureIndex < 0)
			return;

		if (startupWarningShouldOpen)
		{
			startupWarningDontShowAgain = false;
			ImGui::OpenPopup(MODAL_TITLE("startup_warning"));
			startupWarningShouldOpen = false;
		}

		const Texture& warningTexture = ResourceManager::textures[startupWarningTextureIndex];
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImVec2 viewportSize = viewport->WorkSize;
		const float maxImageWidth = std::min(viewportSize.x * 0.68f, 980.0f);
		const float maxImageHeight = std::min(viewportSize.y * 0.58f, 620.0f);
		const float widthScale = maxImageWidth / static_cast<float>(warningTexture.getWidth());
		const float heightScale = maxImageHeight / static_cast<float>(warningTexture.getHeight());
		const float imageScale = std::min(1.0f, std::min(widthScale, heightScale));
		const ImVec2 imageSize{
			warningTexture.getWidth() * imageScale,
			warningTexture.getHeight() * imageScale
		};

		ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowViewport(viewport->ID);
		bool popupOpen = true;
		if (ImGui::BeginPopupModal(MODAL_TITLE("startup_warning"), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
		{
			bool shouldClose = false;

			ImGui::SetCursorPosX(std::max((ImGui::GetContentRegionAvail().x - imageSize.x) * 0.5f, 0.0f));
			ImGui::Image((ImTextureID)(intptr_t)warningTexture.getID(), imageSize);

			ImGui::Spacing();
			ImGui::Checkbox(getString("startup_warning_hide"), &startupWarningDontShowAgain);
			ImGui::Spacing();

			if (ImGui::Button(getString("startup_warning_confirm"), { ImGui::GetContentRegionAvail().x, 0.0f }))
				shouldClose = true;

			if (!popupOpen)
				shouldClose = true;

			if (shouldClose)
			{
				config.hideStartupWarning = startupWarningDontShowAgain;
				config.write(userDataDir + APP_CONFIG_FILENAME);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void Application::loadResources()
	{
		ResourceManager::loadShader(appDir + "res/shaders/basic2d");
		ResourceManager::loadShader(appDir + "res/shaders/masking");
		ResourceManager::loadShader(appDir + "res/shaders/particles");

		// TODO: Do not set the note skin texture indexes manually!
		const std::string notes01TexDir = appDir + "res/notes/01/";
		ResourceManager::loadTexture(notes01TexDir + "notes.png");
		ResourceManager::loadTexture(notes01TexDir + "longNoteLine.png");
		ResourceManager::loadTexture(notes01TexDir + "touchLine_eff.png");
		noteSkins.add("Notes 01", 0, 1, 2);

		const std::string notes02TexDir = appDir + "res/notes/02/";
		ResourceManager::loadTexture(notes02TexDir + "notes.png");
		ResourceManager::loadTexture(notes02TexDir + "longNoteLine.png");
		ResourceManager::loadTexture(notes02TexDir + "touchLine_eff.png");
		noteSkins.add("Notes 02", 3, 4, 5);

		const std::string editorAssetsDir = appDir + "res/editor/";
		ResourceManager::loadTexture(editorAssetsDir + "timeline_tools.png");
		ResourceManager::loadTexture(editorAssetsDir + "note_stats.png");
		ResourceManager::loadTexture(editorAssetsDir + "stage.png");

		const std::string startupWarningImage = appDir + "res/warning.jpg";
		ResourceManager::loadTexture(startupWarningImage);
		startupWarningTextureIndex = ResourceManager::getTextureByFilename(startupWarningImage);

		ResourceManager::loadTransforms(appDir + "res/effect/transform.txt");

		// Load more languages here
		Localization::loadDefault();
		Localization::load("ja", u8"日本語", appDir + "res/i18n/ja.csv");
		Localization::load("zh-cn", u8"简体中文（中国大陆）", appDir + "res/i18n/zh-cn.csv");
		Localization::load("zh-tw", u8"繁體中文（台灣）", appDir + "res/i18n/zh-tw.csv");
	}

	void Application::setFullScreen(bool fullScreen)
	{
		Application::windowState.fullScreen = config.fullScreen = fullScreen;

		GLFWmonitor* mainMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(mainMonitor);
		if (fullScreen)
		{
			if (Application::windowState.maximized)
				glfwSetWindowAttrib(window, GLFW_MAXIMIZED, GLFW_FALSE);

			glfwSetWindowMonitor(window, mainMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		else
		{
			int width = Application::windowState.maximized ? mode->width : Application::windowState.size.x;
			int height = Application::windowState.maximized ? mode->height : Application::windowState.size.y;

			glfwSetWindowMonitor(
				window,
				NULL,
				Application::windowState.position.x,
				Application::windowState.position.y,
				width,
				height,
				mode->refreshRate
			);
		}
	}

	void Application::run()
	{
#ifdef _WIN32
		HWND hwnd = glfwGetWin32Window(window);

		/*
			Override the current GLFW/Imgui window procedure and store it in the GLFW window user pointer
		
			NOTE: For this to be safe, it should be only called AFTER ImGui is initialized
			so that the WndProc ImGui is expecting matches with our own WndProc
		*/
		glfwSetWindowUserPointer(window, (void*)::GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
		::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);

		windowState.windowHandle = hwnd;
		windowState.windowTimerId = ::SetTimer(hwnd,
			reinterpret_cast<UINT_PTR>(&windowState.windowTimerId), USER_TIMER_MINIMUM, nullptr);

		::DragAcceptFiles(hwnd, TRUE);
#else
		windowState.windowHandle = nullptr;
#endif

		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			update();
		}
		
		writeSettings();
	}

	bool Application::attemptSave()
	{
		return editor && editor->trySave(editor->getWorkingFilename().data());
	}

	bool Application::isEditorUpToDate() const
	{
		return editor->isUpToDate();
	}
}
