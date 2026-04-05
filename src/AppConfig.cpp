#define UNICODE

#include "AppConfig.h"

#include <algorithm>
#include <cwctype>
#include <vector>

#include <windows.h>

namespace clipass {

namespace {

constexpr wchar_t kConfigFileName[] = L"clipass.ini";
constexpr wchar_t kIconFileName[] = L"clipass.ico";
constexpr wchar_t kItemsSectionName[] = L"Items";
constexpr wchar_t kSettingsSectionName[] = L"Settings";
constexpr DWORD kIniBufferSize = 32768;

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
        case L'r':
            decoded.push_back(L'\r');
            break;
        case L't':
            decoded.push_back(L'\t');
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

std::wstring ReadIniString(const std::filesystem::path &configPath,
                           const wchar_t *section, const wchar_t *key,
                           const wchar_t *defaultValue) {
    std::vector<wchar_t> buffer(256, L'\0');
    GetPrivateProfileStringW(section, key, defaultValue, buffer.data(),
                             static_cast<DWORD>(buffer.size()),
                             configPath.c_str());
    return std::wstring(buffer.data());
}

bool ReadIniBool(const std::filesystem::path &configPath,
                 const wchar_t *section, const wchar_t *key,
                 bool defaultValue) {
    const std::wstring value = ReadIniString(configPath, section, key,
                                             defaultValue ? L"true" : L"false");

    if (_wcsicmp(value.c_str(), L"true") == 0 ||
        wcscmp(value.c_str(), L"1") == 0 ||
        _wcsicmp(value.c_str(), L"yes") == 0) {
        return true;
    }

    if (_wcsicmp(value.c_str(), L"false") == 0 ||
        wcscmp(value.c_str(), L"0") == 0 ||
        _wcsicmp(value.c_str(), L"no") == 0) {
        return false;
    }

    return defaultValue;
}

std::vector<ClipItem> LoadItems(const std::filesystem::path &configPath) {
    std::vector<ClipItem> items;
    std::vector<wchar_t> itemsBuffer(kIniBufferSize, L'\0');

    GetPrivateProfileSectionW(kItemsSectionName, itemsBuffer.data(),
                              static_cast<DWORD>(itemsBuffer.size()),
                              configPath.c_str());

    for (const wchar_t *current = itemsBuffer.data(); *current != L'\0';
         current += wcslen(current) + 1) {
        const std::wstring entry(current);
        const size_t separator = entry.find(L'=');
        if (separator == std::wstring::npos) {
            continue;
        }

        ClipItem item;
        item.key = TrimCopy(entry.substr(0, separator));
        item.value = DecodeEscapedValue(entry.substr(separator + 1));

        if (!item.key.empty()) {
            items.push_back(std::move(item));
        }
    }

    return items;
}

} // namespace

AppConfig LoadAppConfig() {
    AppConfig config;
    config.executableDirectory = GetExecutableDirectory();
    config.configPath = config.executableDirectory / kConfigFileName;
    config.iconPath = config.executableDirectory / kIconFileName;

    config.settings.hotkeyText =
        ReadIniString(config.configPath, kSettingsSectionName, L"Hotkey",
                      config.settings.hotkeyText.c_str());
    config.settings.autoClose =
        ReadIniBool(config.configPath, kSettingsSectionName, L"AutoClose",
                    config.settings.autoClose);
    config.settings.autoPaste =
        ReadIniBool(config.configPath, kSettingsSectionName, L"AutoPaste",
                    config.settings.autoPaste);
    config.items = LoadItems(config.configPath);

    return config;
}

} // namespace clipass
