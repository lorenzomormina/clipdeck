#include "ClipDeckApp.h"

#include <utility>

namespace ClipDeck {

ClipDeckApp::ClipDeckApp(HINSTANCE instance, AppConfig config)
    : instance_(instance), config_(std::move(config)),
      settingsWindow_(instance_), mainWindow_(instance_, config_) {
    settingsWindow_.SetConfig(config_);
    settingsWindow_.SetConfigSavedCallback([this]() { ReloadConfig(); });

    mainWindow_.SetOpenSettingsCallback(
        [this]() { return OpenSettingsWindow(); });
    mainWindow_.SetReloadConfigCallback([this]() { ReloadConfig(); });
    mainWindow_.SetSettingsActiveCallback(
        [this]() { return settingsWindow_.IsActive(); });
}

int ClipDeckApp::Run(int nCmdShow) {
    if (!mainWindow_.Create(nCmdShow)) {
        return 0;
    }

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        if (settingsWindow_.PreTranslateMessage(message)) {
            continue;
        }

        if (mainWindow_.PreTranslateMessage(message)) {
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

bool ClipDeckApp::OpenSettingsWindow() { return settingsWindow_.Show(); }

void ClipDeckApp::ReloadConfig() {
    config_ = LoadAppConfig();
    settingsWindow_.SetConfig(config_);
    mainWindow_.ApplyConfig();
}

} // namespace ClipDeck
