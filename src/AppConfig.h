#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ClipDeck {

struct ClipItem {
    std::wstring key;
    std::wstring value;
    bool hidden = false;
    std::optional<bool> searchValues;
    std::optional<bool> autoClose;
    std::optional<bool> autoPaste;
    std::optional<bool> caseSensitiveKeys;
    std::optional<bool> caseSensitiveValues;

    std::wstring GetDisplayText() const {
        if (hidden) {
            return L"[" + key + L"] " + L"*****";
        }
        return L"[" + key + L"] " + value;
    }
};

struct AppSettings {
    bool startHidden = true;
};

struct HotkeySettings {
    std::wstring open = L"Ctrl+Shift+Space";
};

struct MainWindowSettings {
    int width = 400;
    int height = 300;
    int margin = 4;
    int textBoxMargin = 6;
    bool hideOnBlur = true;
    bool keepVisibleWhileConfiguring = true;
};

struct SettingsWindowSettings {
    int width = 600;
    int height = 400;
    int margin = 4;
};

struct ActivationSettings {
    bool autoClose = true;
    bool autoPaste = false;
};

struct SearchSettings {
    bool searchValues = false;
    bool caseSensitiveKeys = false;
    bool caseSensitiveValues = true;
};

struct AppConfig {
    std::filesystem::path executableDirectory;
    std::filesystem::path settingsPath;
    AppSettings appSettings;
    HotkeySettings hotkeySettings;
    MainWindowSettings mainWindowSettings;
    SettingsWindowSettings settingsWindowSettings;
    ActivationSettings activationSettings;
    SearchSettings searchSettings;
    std::vector<ClipItem> items;
};

AppConfig LoadAppConfig();

} // namespace ClipDeck
