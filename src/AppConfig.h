#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace clipass {

struct ClipItem {
    std::wstring key;
    std::wstring value;
    bool hidden = false;
    bool enableSearchValue = false;

    std::wstring GetDisplayText() const {
        if (hidden) {
            return L"[" + key + L"] " + L"*****";
        }
        return L"[" + key + L"] " + value;
    }
};

struct GeneralSettings {
    std::wstring hotkeyText = L"Ctrl+Shift+Space";
    bool startHidden = true;
    bool autoClose = true;
    bool autoPaste = false;
};

struct WindowSettings {
    int width = 400;
    int height = 300;
    int margin = 4;
    int textBoxMargin = 6;
};

struct ConfigWindowSettings {
    int width = 600;
    int height = 400;
    int margin = 4;
};

struct AppConfig {
    std::filesystem::path executableDirectory;
    std::filesystem::path configPath;
    std::filesystem::path iconPath;
    GeneralSettings generalSettings;
    WindowSettings windowSettings;
    ConfigWindowSettings configWindowSettings;
    std::vector<ClipItem> items;
};

AppConfig LoadAppConfig();

} // namespace clipass
