#include "AppConfig.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <vector>

#include <windows.h>

namespace ClipDeck {

namespace {

constexpr wchar_t kConfigFileName[] = L"config.txt";
constexpr wchar_t kIconFileName[] = L"icon.ico";

enum class ParseSection {
    None,
    General,
    Window,
    ConfigWindow,
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

    // Fallback for legacy non-UTF-8 config files.
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
    return !key->empty() && !value->empty();
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

void ApplyGeneralSetting(AppConfig *config, const std::wstring &key,
                         const std::wstring &valueToken) {
    if (key == L"Hotkey") {
        std::wstring parsed;
        if (ParseQuotedString(valueToken, &parsed)) {
            config->generalSettings.hotkeyText = parsed;
        }
        return;
    }

    bool parsed = false;
    if (key == L"StartHidden") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->generalSettings.startHidden = parsed;
        }
    } else if (key == L"AutoClose") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->generalSettings.autoClose = parsed;
        }
    } else if (key == L"AutoPaste") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->generalSettings.autoPaste = parsed;
        }
    } else if (key == L"EnableValueSearch") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->generalSettings.enableValueSearch = parsed;
        }
    } else if (key == L"HideOnBlur") {
        if (ParseBoolValue(valueToken, &parsed)) {
            config->generalSettings.hideOnBlur = parsed;
        }
    }
}

void ApplyWindowSetting(AppConfig *config, const std::wstring &key,
                        const std::wstring &valueToken) {
    int parsed = 0;
    if (key == L"Width") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->windowSettings.width = parsed;
        }
    } else if (key == L"Height") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->windowSettings.height = parsed;
        }
    } else if (key == L"Margin") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->windowSettings.margin = parsed;
        }
    } else if (key == L"TextBoxMargin") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->windowSettings.textBoxMargin = parsed;
        }
    }
}

void ApplyConfigWindowSetting(AppConfig *config, const std::wstring &key,
                              const std::wstring &valueToken) {
    int parsed = 0;
    if (key == L"Width") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->configWindowSettings.width = parsed;
        }
    } else if (key == L"Height") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->configWindowSettings.height = parsed;
        }
    } else if (key == L"Margin") {
        if (ParseIntValue(valueToken, &parsed)) {
            config->configWindowSettings.margin = parsed;
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
    } else if (key == L"EnableValueSearch") {
        if (ParseBoolValue(valueToken, &parsed)) {
            item->enableValueSearch = parsed;
        }
    } else if (key == L"AutoClose") {
        if (ParseBoolValue(valueToken, &parsed)) {
            item->autoClose = parsed;
        }
    } else if (key == L"AutoPaste") {
        if (ParseBoolValue(valueToken, &parsed)) {
            item->autoPaste = parsed;
        }
    }
}

void ParseConfigFile(const std::filesystem::path &configPath,
                     AppConfig *config) {
    std::wstring configText;
    if (!ReadTextFileWithEncodingFallback(configPath, &configText)) {
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
            if (line == L"[General]") {
                currentSection = ParseSection::General;
                currentItem = nullptr;
            } else if (line == L"[Window]") {
                currentSection = ParseSection::Window;
                currentItem = nullptr;
            } else if (line == L"[ConfigWindow]") {
                currentSection = ParseSection::ConfigWindow;
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
        case ParseSection::General:
            ApplyGeneralSetting(config, key, valueToken);
            break;
        case ParseSection::Window:
            ApplyWindowSetting(config, key, valueToken);
            break;
        case ParseSection::ConfigWindow:
            ApplyConfigWindowSetting(config, key, valueToken);
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
    config.configPath = config.executableDirectory / kConfigFileName;
    config.iconPath = config.executableDirectory / "icons" / kIconFileName;

    ParseConfigFile(config.configPath, &config);

    return config;
}

} // namespace ClipDeck
