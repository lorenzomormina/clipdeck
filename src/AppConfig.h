#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace clipass {

struct ClipItem {
    std::wstring key;
    std::wstring value;
    bool hidden = false;

    std::wstring GetDisplayText() const {
        if (hidden) {
            return L"[" + key + L"] " + L"*****";
        }
        return L"[" + key + L"] " + value;
    }
};

struct AppSettings {
    std::wstring hotkeyText = L"Ctrl+Shift+Space";
    bool startHidden = true;
    bool autoClose = true;
    bool autoPaste = false;
};

struct AppConfig {
    std::filesystem::path executableDirectory;
    std::filesystem::path configPath;
    std::filesystem::path iconPath;
    AppSettings settings;
    std::vector<ClipItem> items;
};

AppConfig LoadAppConfig();

} // namespace clipass
