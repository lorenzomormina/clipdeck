#include "AppConfig.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <vector>

#include <windows.h>

namespace ClipDeck {

namespace {

constexpr wchar_t kConfigFileName[] = L"settings.txt";
constexpr wchar_t kIconFileName[] = L"icon.ico";

enum class ParseSection {
    None,
    App,
    Hotkey,
    MainWindow,
    SettingsWindow,
    Activation,
    Search,
    Item,
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

void ApplyItemSetting(ClipItem *item, const std::wstring &key,
                      const std::wstring &valueToken) {
    if (key == L"Key") {
        std::wstring parsed;
        if (ParseQuotedString(valueToken, &parsed)) {
            item->key = parsed;
        }
        return;
    }

    if (key == L"Value") {
        std::wstring parsed;
        if (ParseQuotedString(valueToken, &parsed)) {
            item->value = parsed;
        }
        return;
    }

    bool parsed = false;
    if (key == L"Hidden") {
        if (ParseBoolValue(valueToken, &parsed)) {
            item->hidden = parsed;
        }
    } else if (key == L"Search.SearchValues") {
        if (valueToken.empty()) {
            item->searchValues.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->searchValues = parsed;
        }
    } else if (key == L"Search.CaseSensitiveSearchKeys") {
        if (valueToken.empty()) {
            item->caseSensitiveSearchKeys.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->caseSensitiveSearchKeys = parsed;
        }
    } else if (key == L"Search.CaseSensitiveSearchValues") {
        if (valueToken.empty()) {
            item->caseSensitiveSearchValues.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->caseSensitiveSearchValues = parsed;
        }
    } else if (key == L"Search.AdvancedSearchKeys") {
        if (valueToken.empty()) {
            item->advancedSearchKeys.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->advancedSearchKeys = parsed;
        }
    } else if (key == L"Search.AdvancedSearchValues") {
        if (valueToken.empty()) {
            item->advancedSearchValues.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->advancedSearchValues = parsed;
        }
    } else if (key == L"Activation.AutoClose") {
        if (valueToken.empty()) {
            item->autoClose.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->autoClose = parsed;
        }
    } else if (key == L"Activation.AutoPaste") {
        if (valueToken.empty()) {
            item->autoPaste.reset();
        } else if (ParseBoolValue(valueToken, &parsed)) {
            item->autoPaste = parsed;
        }
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
    ClipItem *currentItem = nullptr;

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
                currentItem = nullptr;
            } else if (line == L"[Hotkey]") {
                currentSection = ParseSection::Hotkey;
                currentItem = nullptr;
            } else if (line == L"[MainWindow]") {
                currentSection = ParseSection::MainWindow;
                currentItem = nullptr;
            } else if (line == L"[SettingsWindow]") {
                currentSection = ParseSection::SettingsWindow;
                currentItem = nullptr;
            } else if (line == L"[Activation]") {
                currentSection = ParseSection::Activation;
                currentItem = nullptr;
            } else if (line == L"[Search]") {
                currentSection = ParseSection::Search;
                currentItem = nullptr;
            } else if (line == L"[[Item]]") {
                currentSection = ParseSection::Item;
                config->items.emplace_back();
                currentItem = &config->items.back();
            } else {
                currentSection = ParseSection::None;
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
        case ParseSection::Item:
            if (currentItem != nullptr) {
                ApplyItemSetting(currentItem, key, valueToken);
            }
            break;
        case ParseSection::None:
            break;
        }
    }
}

} // namespace

AppConfig LoadAppConfig() {
    AppConfig config;
    config.executableDirectory = GetExecutableDirectory();
    config.settingsPath = config.executableDirectory / kConfigFileName;

    ParseConfigFile(config.settingsPath, &config);

    return config;
}

} // namespace ClipDeck
