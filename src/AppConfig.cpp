#include "AppConfig.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include <windows.h>

namespace ClipDeck {

namespace {

constexpr wchar_t kConfigFileName[] = L"settings.txt";

enum class ParseSection {
    None,
    App,
    Hotkey,
    MainWindow,
    SettingsWindow,
    Activation,
    Search,
    Group,
    Item,
};

struct ParsedGroup {
    std::wstring key;
    std::wstring name;
    std::optional<bool> hidden;
    std::optional<bool> searchValues;
    std::optional<bool> autoClose;
    std::optional<bool> autoPaste;
    std::optional<bool> caseSensitiveSearchKeys;
    std::optional<bool> caseSensitiveSearchValues;
    std::optional<bool> advancedSearchKeys;
    std::optional<bool> advancedSearchValues;
};

struct ParsedItem {
    ClipItemType type = ClipItemType::Text;
    std::wstring group;
    std::wstring key;
    std::wstring value;
    std::wstring path;
    std::wstring displayText;
    FileLoadMode loadMode = FileLoadMode::OnActivation;
    std::optional<bool> hidden;
    std::optional<bool> searchValues;
    std::optional<bool> autoClose;
    std::optional<bool> autoPaste;
    std::optional<bool> caseSensitiveSearchKeys;
    std::optional<bool> caseSensitiveSearchValues;
    std::optional<bool> advancedSearchKeys;
    std::optional<bool> advancedSearchValues;
    size_t loadOrder = 0;
};

std::filesystem::path GetExecutableDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

std::wstring TrimCopy(const std::wstring &value) {
    const auto first =
        std::find_if_not(value.begin(), value.end(), [](wchar_t character) {
            return iswspace(character) != 0;
        });

    const auto last =
        std::find_if_not(value.rbegin(), value.rend(), [](wchar_t character) {
            return iswspace(character) != 0;
        }).base();

    if (first >= last) {
        return L"";
    }

    return std::wstring(first, last);
}

std::wstring DecodeEscapedValue(const std::wstring &rawValue) {
    std::wstring decoded;
    decoded.reserve(rawValue.size());

    for (size_t index = 0; index < rawValue.size(); ++index) {
        const wchar_t current = rawValue[index];
        if (current != L'\\' || index + 1 >= rawValue.size()) {
            decoded.push_back(current);
            continue;
        }

        const wchar_t escape = rawValue[++index];
        switch (escape) {
        case L'n':
            decoded.push_back(L'\n');
            break;
        case L'"':
            decoded.push_back(L'"');
            break;
        case L'\\':
            decoded.push_back(L'\\');
            break;
        default:
            decoded.push_back(escape);
            break;
        }
    }

    return decoded;
}

std::wstring ToLowerCopy(const std::wstring &value) {
    std::wstring lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](wchar_t character) {
                       return static_cast<wchar_t>(std::towlower(character));
                   });
    return lowered;
}

std::wstring DecodeBytesToWide(const std::string &bytes, UINT codePage,
                               DWORD flags) {
    if (bytes.empty()) {
        return L"";
    }

    const int requiredChars =
        MultiByteToWideChar(codePage, flags, bytes.data(),
                            static_cast<int>(bytes.size()), nullptr, 0);
    if (requiredChars <= 0) {
        return L"";
    }

    std::wstring converted(static_cast<size_t>(requiredChars), L'\0');
    const int writtenChars = MultiByteToWideChar(
        codePage, flags, bytes.data(), static_cast<int>(bytes.size()),
        converted.data(), requiredChars);
    if (writtenChars <= 0) {
        return L"";
    }

    return converted;
}

bool ReadTextFileWithEncodingFallback(const std::filesystem::path &path,
                                      std::wstring *text) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }

    std::wstring utf8Decoded =
        DecodeBytesToWide(bytes, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!utf8Decoded.empty() || bytes.empty()) {
        *text = std::move(utf8Decoded);
        return true;
    }

    // Fallback for legacy non-UTF-8 settings files.
    *text = DecodeBytesToWide(bytes, CP_ACP, 0);
    return true;
}

std::wstring StripInlineComment(const std::wstring &line) {
    bool inQuotes = false;
    bool escaped = false;

    for (size_t index = 0; index < line.size(); ++index) {
        const wchar_t current = line[index];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (current == L'\\' && inQuotes) {
            escaped = true;
            continue;
        }

        if (current == L'"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && (current == L'#' || current == L';')) {
            return line.substr(0, index);
        }
    }

    return line;
}

bool ParseKeyValue(const std::wstring &line, std::wstring *key,
                   std::wstring *value) {
    const size_t separator = line.find(L'=');
    if (separator == std::wstring::npos) {
        return false;
    }

    *key = TrimCopy(line.substr(0, separator));
    *value = TrimCopy(line.substr(separator + 1));
    return !key->empty();
}

bool ParseQuotedString(const std::wstring &valueToken, std::wstring *parsed) {
    if (valueToken.size() < 2 || valueToken.front() != L'"' ||
        valueToken.back() != L'"') {
        return false;
    }

    const std::wstring rawValue = valueToken.substr(1, valueToken.size() - 2);

    // Ensure quote characters are escaped inside string content.
    bool escaped = false;
    for (wchar_t character : rawValue) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (character == L'\\') {
            escaped = true;
            continue;
        }

        if (character == L'"') {
            return false;
        }
    }

    *parsed = DecodeEscapedValue(rawValue);
    return true;
}

bool ParseBoolValue(const std::wstring &valueToken, bool *parsed) {
    const std::wstring lowered = ToLowerCopy(valueToken);
    if (lowered == L"true") {
        *parsed = true;
        return true;
    }

    if (lowered == L"false") {
        *parsed = false;
        return true;
    }

    return false;
}

bool ParseIntValue(const std::wstring &valueToken, int *parsed) {
    if (valueToken.empty()) {
        return false;
    }

    size_t consumed = 0;
    try {
        const int value = std::stoi(valueToken, &consumed, 10);
        if (consumed != valueToken.size()) {
            return false;
        }
        *parsed = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseConfigString(const std::wstring &valueToken, std::wstring *parsed) {
    if (ParseQuotedString(valueToken, parsed)) {
        return true;
    }

    if (!valueToken.empty() && valueToken.front() == L'"') {
        return false;
    }

    *parsed = valueToken;
    return true;
}

ClipItemType ParseClipItemType(const std::wstring &valueToken) {
    std::wstring parsed;
    if (!ParseConfigString(valueToken, &parsed)) {
        return ClipItemType::Text;
    }

    const std::wstring lowered = ToLowerCopy(parsed);
    if (lowered == L"file") {
        return ClipItemType::File;
    }

    return ClipItemType::Text;
}

FileLoadMode ParseFileLoadMode(const std::wstring &valueToken) {
    std::wstring parsed;
    if (!ParseConfigString(valueToken, &parsed)) {
        return FileLoadMode::OnActivation;
    }

    const std::wstring lowered = ToLowerCopy(parsed);
    if (lowered == L"lazy") {
        return FileLoadMode::Lazy;
    }

    if (lowered == L"eager") {
        return FileLoadMode::Eager;
    }

    return FileLoadMode::OnActivation;
}

std::wstring ResolveConfiguredPath(const std::wstring &pathText,
                                   const AppConfig &config) {
    if (pathText.empty()) {
        return L"";
    }

    std::filesystem::path path(pathText);
    if (path.is_relative()) {
        path = config.settingsPath.parent_path() / path;
    }

    return path.lexically_normal().wstring();
}

void ApplyOptionalBoolSetting(std::optional<bool> *target,
                              const std::wstring &valueToken) {
    if (valueToken.empty()) {
        target->reset();
        return;
    }

    bool parsed = false;
    if (ParseBoolValue(valueToken, &parsed)) {
        *target = parsed;
    }
}

void ApplyAppSetting(AppConfig *config, const std::wstring &key,
                     const std::wstring &valueToken) {
    bool parsed = false;
    if (key == L"StartHidden") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->appSettings.startHidden = parsed;
        }
    }
}

void ApplyHotkeySetting(AppConfig *config, const std::wstring &key,
                        const std::wstring &valueToken) {
    if (key == L"Open") {
        std::wstring parsed;
        if (ParseQuotedString(valueToken, &parsed)) {
            config->hotkeySettings.open = parsed;
        }
    }
}

void ApplyMainWindowSetting(AppConfig *config, const std::wstring &key,
                            const std::wstring &valueToken) {
    bool parsed = false;
    int parsedInt = 0;
    if (key == L"Width") {
        if (ParseIntValue(valueToken, &parsedInt)) {
            config->mainWindowSettings.width = parsedInt;
        }
    } else if (key == L"Height") {
        if (ParseIntValue(valueToken, &parsedInt)) {
            config->mainWindowSettings.height = parsedInt;
        }
    } else if (key == L"Margin") {
        if (ParseIntValue(valueToken, &parsedInt)) {
            config->mainWindowSettings.margin = parsedInt;
        }
    } else if (key == L"TextBoxMargin") {
        if (ParseIntValue(valueToken, &parsedInt)) {
            config->mainWindowSettings.textBoxMargin = parsedInt;
        }
    } else if (key == L"HideOnBlur") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->mainWindowSettings.hideOnBlur = parsed;
        }
    } else if (key == L"KeepVisibleWhileConfiguring") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->mainWindowSettings.keepVisibleWhileConfiguring = parsed;
        }
    } else if (key == L"GroupListBoxWidth") {
        if (ParseIntValue(valueToken, &parsedInt)) {
            config->mainWindowSettings.groupListBoxWidth = parsedInt;
        }
    }
}

void ApplySettingsWindowSetting(AppConfig *config, const std::wstring &key,
                                const std::wstring &valueToken) {
    int parsed = 0;
    if (key == L"Width") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->settingsWindowSettings.width = parsed;
        }
    } else if (key == L"Height") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->settingsWindowSettings.height = parsed;
        }
    } else if (key == L"Margin") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->settingsWindowSettings.margin = parsed;
        }
    }
}

void ApplyActivationSetting(AppConfig *config, const std::wstring &key,
                            const std::wstring &valueToken) {
    bool parsed = false;
    if (key == L"AutoClose") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->activationSettings.autoClose = parsed;
        }
    } else if (key == L"AutoPaste") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->activationSettings.autoPaste = parsed;
        }
    }
}

void ApplySearchSetting(AppConfig *config, const std::wstring &key,
                        const std::wstring &valueToken) {
    bool parsed = false;
    if (key == L"SearchValues") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->searchSettings.searchValues = parsed;
        }
    } else if (key == L"CaseSensitiveSearchKeys") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->searchSettings.caseSensitiveSearchKeys = parsed;
        }
    } else if (key == L"CaseSensitiveSearchValues") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->searchSettings.caseSensitiveSearchValues = parsed;
        }
    } else if (key == L"AdvancedSearchKeys") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->searchSettings.advancedSearchKeys = parsed;
        }
    } else if (key == L"AdvancedSearchValues") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->searchSettings.advancedSearchValues = parsed;
        }
    }
}

void ApplyGroupSetting(ParsedGroup *group, const std::wstring &key,
                       const std::wstring &valueToken) {
    if (key == L"Key") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            group->key = parsed;
        }
        return;
    }

    if (key == L"Name") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            group->name = parsed;
        }
        return;
    }

    if (key == L"Hidden") {
        ApplyOptionalBoolSetting(&group->hidden, valueToken);
    } else if (key == L"Search.SearchValues") {
        ApplyOptionalBoolSetting(&group->searchValues, valueToken);
    } else if (key == L"Search.CaseSensitiveSearchKeys") {
        ApplyOptionalBoolSetting(&group->caseSensitiveSearchKeys, valueToken);
    } else if (key == L"Search.CaseSensitiveSearchValues") {
        ApplyOptionalBoolSetting(&group->caseSensitiveSearchValues, valueToken);
    } else if (key == L"Search.AdvancedSearchKeys") {
        ApplyOptionalBoolSetting(&group->advancedSearchKeys, valueToken);
    } else if (key == L"Search.AdvancedSearchValues") {
        ApplyOptionalBoolSetting(&group->advancedSearchValues, valueToken);
    } else if (key == L"Activation.AutoClose") {
        ApplyOptionalBoolSetting(&group->autoClose, valueToken);
    } else if (key == L"Activation.AutoPaste") {
        ApplyOptionalBoolSetting(&group->autoPaste, valueToken);
    }
}

void ApplyItemSetting(ParsedItem *item, const std::wstring &key,
                      const std::wstring &valueToken) {
    if (key == L"Type") {
        item->type = ParseClipItemType(valueToken);
        return;
    }

    if (key == L"Group") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            item->group = parsed;
        }
        return;
    }

    if (key == L"Key") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            item->key = parsed;
        }
        return;
    }

    if (key == L"Value") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            item->value = parsed;
        }
        return;
    }

    if (key == L"Path") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            item->path = parsed;
        }
        return;
    }

    if (key == L"DisplayText") {
        std::wstring parsed;
        if (ParseConfigString(valueToken, &parsed)) {
            item->displayText = parsed;
        }
        return;
    }

    if (key == L"LoadMode") {
        item->loadMode = ParseFileLoadMode(valueToken);
        return;
    }

    if (key == L"Hidden") {
        ApplyOptionalBoolSetting(&item->hidden, valueToken);
    } else if (key == L"Search.SearchValues") {
        ApplyOptionalBoolSetting(&item->searchValues, valueToken);
    } else if (key == L"Search.CaseSensitiveSearchKeys") {
        ApplyOptionalBoolSetting(&item->caseSensitiveSearchKeys, valueToken);
    } else if (key == L"Search.CaseSensitiveSearchValues") {
        ApplyOptionalBoolSetting(&item->caseSensitiveSearchValues, valueToken);
    } else if (key == L"Search.AdvancedSearchKeys") {
        ApplyOptionalBoolSetting(&item->advancedSearchKeys, valueToken);
    } else if (key == L"Search.AdvancedSearchValues") {
        ApplyOptionalBoolSetting(&item->advancedSearchValues, valueToken);
    } else if (key == L"Activation.AutoClose") {
        ApplyOptionalBoolSetting(&item->autoClose, valueToken);
    } else if (key == L"Activation.AutoPaste") {
        ApplyOptionalBoolSetting(&item->autoPaste, valueToken);
    }
}

std::optional<size_t> FindGroupIndex(const std::vector<Group> &groups,
                                     const std::wstring &key) {
    for (size_t index = 0; index < groups.size(); ++index) {
        if (groups[index].key == key) {
            return index;
        }
    }

    return std::nullopt;
}

Group MakeDefaultGroup(const AppConfig &config) {
    Group group;
    group.key = defaultGroupKey;
    group.name = defaultGroupName;
    group.hidden = false;
    group.searchValues = config.searchSettings.searchValues;
    group.autoClose = config.activationSettings.autoClose;
    group.autoPaste = config.activationSettings.autoPaste;
    group.caseSensitiveSearchKeys =
        config.searchSettings.caseSensitiveSearchKeys;
    group.caseSensitiveSearchValues =
        config.searchSettings.caseSensitiveSearchValues;
    group.advancedSearchKeys = config.searchSettings.advancedSearchKeys;
    group.advancedSearchValues = config.searchSettings.advancedSearchValues;
    return group;
}

Group ResolveGroup(const ParsedGroup &parsed, const AppConfig &config) {
    Group group;
    group.key = parsed.key;
    group.name = parsed.name.empty() ? parsed.key : parsed.name;
    group.hidden = parsed.hidden.value_or(false);
    group.searchValues =
        parsed.searchValues.value_or(config.searchSettings.searchValues);
    group.autoClose =
        parsed.autoClose.value_or(config.activationSettings.autoClose);
    group.autoPaste =
        parsed.autoPaste.value_or(config.activationSettings.autoPaste);
    group.caseSensitiveSearchKeys = parsed.caseSensitiveSearchKeys.value_or(
        config.searchSettings.caseSensitiveSearchKeys);
    group.caseSensitiveSearchValues = parsed.caseSensitiveSearchValues.value_or(
        config.searchSettings.caseSensitiveSearchValues);
    group.advancedSearchKeys = parsed.advancedSearchKeys.value_or(
        config.searchSettings.advancedSearchKeys);
    group.advancedSearchValues = parsed.advancedSearchValues.value_or(
        config.searchSettings.advancedSearchValues);
    return group;
}

ClipItem ResolveItem(const ParsedItem &parsed, const Group &group,
                     const AppConfig &config) {
    ClipItem item;
    item.type = parsed.type;
    item.group = group.key;
    item.key = parsed.key;
    item.value = parsed.value;
    item.path = ResolveConfiguredPath(parsed.path, config);
    item.displayText = parsed.displayText;
    item.loadMode = parsed.loadMode;
    item.hidden = parsed.hidden.value_or(group.hidden);
    item.searchValues = parsed.searchValues.value_or(group.searchValues);
    item.autoClose = parsed.autoClose.value_or(group.autoClose);
    item.autoPaste = parsed.autoPaste.value_or(group.autoPaste);
    item.caseSensitiveSearchKeys =
        parsed.caseSensitiveSearchKeys.value_or(group.caseSensitiveSearchKeys);
    item.caseSensitiveSearchValues = parsed.caseSensitiveSearchValues.value_or(
        group.caseSensitiveSearchValues);
    item.advancedSearchKeys =
        parsed.advancedSearchKeys.value_or(group.advancedSearchKeys);
    item.advancedSearchValues =
        parsed.advancedSearchValues.value_or(group.advancedSearchValues);
    item.loadOrder = parsed.loadOrder;

    if (item.type == ClipItemType::File &&
        item.loadMode == FileLoadMode::Eager && !item.path.empty()) {
        std::wstring fileContent;
        if (ReadTextFileWithEncodingFallback(item.path, &fileContent)) {
            item.cachedFileContent = std::move(fileContent);
        }
    }

    return item;
}

void ResolveGroupsAndItems(AppConfig *config,
                           const std::vector<ParsedGroup> &parsedGroups,
                           const std::vector<ParsedItem> &parsedItems) {
    config->groups.clear();
    config->groups.push_back(MakeDefaultGroup(*config));

    for (const ParsedGroup &parsedGroup : parsedGroups) {
        if (parsedGroup.key.empty() || parsedGroup.key == defaultGroupKey ||
            FindGroupIndex(config->groups, parsedGroup.key)) {
            continue;
        }

        config->groups.push_back(ResolveGroup(parsedGroup, *config));
    }

    for (const ParsedItem &parsedItem : parsedItems) {
        std::optional<size_t> groupIndex;
        if (!parsedItem.group.empty()) {
            groupIndex = FindGroupIndex(config->groups, parsedItem.group);
        }

        if (!groupIndex) {
            groupIndex = 0;
        }

        Group &group = config->groups[*groupIndex];
        group.items.push_back(ResolveItem(parsedItem, group, *config));
    }
}

void ParseConfigFile(const std::filesystem::path &settingsPath,
                     AppConfig *config) {
    std::wstring configText;
    if (!ReadTextFileWithEncodingFallback(settingsPath, &configText)) {
        return;
    }

    std::wistringstream configStream(configText);

    ParseSection currentSection = ParseSection::None;
    ParsedGroup *currentGroup = nullptr;
    ParsedItem *currentItem = nullptr;
    std::vector<ParsedGroup> parsedGroups;
    std::vector<ParsedItem> parsedItems;

    std::wstring rawLine;
    while (std::getline(configStream, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == L'\r') {
            rawLine.pop_back();
        }

        const std::wstring withoutComments = StripInlineComment(rawLine);
        const std::wstring line = TrimCopy(withoutComments);
        if (line.empty()) {
            continue;
        }

        if (line.front() == L'[' && line.back() == L']') {
            if (line == L"[App]") {
                currentSection = ParseSection::App;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[Hotkey]") {
                currentSection = ParseSection::Hotkey;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[MainWindow]") {
                currentSection = ParseSection::MainWindow;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[SettingsWindow]") {
                currentSection = ParseSection::SettingsWindow;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[Activation]") {
                currentSection = ParseSection::Activation;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[Search]") {
                currentSection = ParseSection::Search;
                currentGroup = nullptr;
                currentItem = nullptr;
            } else if (line == L"[[Group]]") {
                currentSection = ParseSection::Group;
                parsedGroups.emplace_back();
                currentGroup = &parsedGroups.back();
                currentItem = nullptr;
            } else if (line == L"[[Item]]") {
                currentSection = ParseSection::Item;
                parsedItems.emplace_back();
                parsedItems.back().loadOrder = parsedItems.size() - 1;
                currentGroup = nullptr;
                currentItem = &parsedItems.back();
            } else {
                currentSection = ParseSection::None;
                currentGroup = nullptr;
                currentItem = nullptr;
            }
            continue;
        }

        std::wstring key;
        std::wstring valueToken;
        if (!ParseKeyValue(line, &key, &valueToken)) {
            continue;
        }

        switch (currentSection) {
        case ParseSection::App:
            ApplyAppSetting(config, key, valueToken);
            break;
        case ParseSection::Hotkey:
            ApplyHotkeySetting(config, key, valueToken);
            break;
        case ParseSection::MainWindow:
            ApplyMainWindowSetting(config, key, valueToken);
            break;
        case ParseSection::SettingsWindow:
            ApplySettingsWindowSetting(config, key, valueToken);
            break;
        case ParseSection::Activation:
            ApplyActivationSetting(config, key, valueToken);
            break;
        case ParseSection::Search:
            ApplySearchSetting(config, key, valueToken);
            break;
        case ParseSection::Group:
            if (currentGroup != nullptr) {
                ApplyGroupSetting(currentGroup, key, valueToken);
            }
            break;
        case ParseSection::Item:
            if (currentItem != nullptr) {
                ApplyItemSetting(currentItem, key, valueToken);
            }
            break;
        case ParseSection::None:
            break;
        }
    }

    ResolveGroupsAndItems(config, parsedGroups, parsedItems);
}

} // namespace

AppConfig LoadAppConfig() {
    AppConfig config;
    config.executableDirectory = GetExecutableDirectory();
    config.settingsPath = config.executableDirectory / kConfigFileName;

    ParseConfigFile(config.settingsPath, &config);
    if (config.groups.empty()) {
        config.groups.push_back(MakeDefaultGroup(config));
    }

    return config;
}

std::wstring GetItemDisplayText(const ClipItem &item) {
    std::wstring preview;
    if (item.hidden) {
        preview = L"*****";
    } else if (!item.displayText.empty()) {
        preview = item.displayText;
    } else if (item.type == ClipItemType::File) {
        const std::filesystem::path path(item.path);
        preview = path.has_filename() ? path.filename().wstring() : item.path;
    } else {
        preview = item.value;
    }

    return L"[" + item.key + L"] " + preview;
}

const std::wstring *GetItemSearchValueText(const ClipItem &item) {
    if (item.hidden || !item.searchValues) {
        return nullptr;
    }

    if (item.type == ClipItemType::Text) {
        return &item.value;
    }

    if (item.cachedFileContent) {
        return &(*item.cachedFileContent);
    }

    return nullptr;
}

bool TryGetActivationText(const ClipItem &item, std::wstring *text,
                          std::wstring *error) {
    if (!text || !error) {
        return false;
    }

    text->clear();
    error->clear();

    if (item.type == ClipItemType::Text) {
        *text = item.value;
        return true;
    }

    if (item.path.empty()) {
        *error = L"No file path is configured for item \"" + item.key + L"\".";
        return false;
    }

    if ((item.loadMode == FileLoadMode::Lazy ||
         item.loadMode == FileLoadMode::Eager) &&
        item.cachedFileContent) {
        *text = *item.cachedFileContent;
        return true;
    }

    std::wstring fileContent;
    if (!ReadTextFileWithEncodingFallback(item.path, &fileContent)) {
        *error = L"Could not read file for item \"" + item.key + L"\":\n\n" +
                 item.path;
        return false;
    }

    if (item.loadMode == FileLoadMode::Lazy ||
        item.loadMode == FileLoadMode::Eager) {
        item.cachedFileContent = fileContent;
    }

    *text = std::move(fileContent);
    return true;
}

} // namespace ClipDeck
