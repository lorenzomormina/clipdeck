#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ClipDeck {

const std::wstring defaultGroupKey = L"default";
const std::wstring defaultGroupName = L"(no group)";

enum class ClipItemType { Text, File };

enum class FileLoadMode { OnActivation, Lazy, Eager };

struct ClipItem {
    ClipItemType type = ClipItemType::Text;
    std::wstring key;
    std::wstring value;
    std::wstring path;
    std::wstring displayText;
    FileLoadMode loadMode = FileLoadMode::OnActivation;
    std::wstring group = defaultGroupKey;
    bool hidden = false;
    bool searchValues = false;
    bool autoClose = true;
    bool autoPaste = false;
    bool caseSensitiveSearchKeys = false;
    bool caseSensitiveSearchValues = true;
    bool advancedSearchKeys = false;
    bool advancedSearchValues = false;
    size_t loadOrder = 0;
    mutable std::optional<std::wstring> cachedFileContent;
};

struct Group {
    std::wstring key = defaultGroupKey;
    std::wstring name = defaultGroupName;
    bool hidden = false;
    bool searchValues = false;
    bool autoClose = true;
    bool autoPaste = false;
    bool caseSensitiveSearchKeys = false;
    bool caseSensitiveSearchValues = true;
    bool advancedSearchKeys = false;
    bool advancedSearchValues = false;
    std::vector<ClipItem> items;
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
    int groupListBoxWidth = 100;
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
    bool caseSensitiveSearchKeys = false;
    bool caseSensitiveSearchValues = true;
    bool advancedSearchKeys = false;
    bool advancedSearchValues = false;
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
    std::vector<Group> groups;
};

AppConfig LoadAppConfig();
std::wstring GetItemDisplayText(const ClipItem &item);
const std::wstring *GetItemSearchValueText(const ClipItem &item);
bool TryGetActivationText(const ClipItem &item, std::wstring *text,
                          std::wstring *error);

} // namespace ClipDeck
