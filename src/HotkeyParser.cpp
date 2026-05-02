#include "HotkeyParser.h"

#include <algorithm>
#include <cwctype>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clipass {

namespace {

std::wstring TrimCopy(std::wstring_view value) {
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

std::wstring ToLowerCopy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t character) {
                       return static_cast<wchar_t>(std::towlower(character));
                   });
    return value;
}

std::vector<std::wstring> SplitHotkeyText(const std::wstring &hotkeyText) {
    std::vector<std::wstring> parts;
    size_t start = 0;

    while (start <= hotkeyText.size()) {
        const size_t separator = hotkeyText.find(L'+', start);
        const size_t end =
            separator == std::wstring::npos ? hotkeyText.size() : separator;
        parts.push_back(TrimCopy(std::wstring_view(hotkeyText).substr(
            start, end - start)));

        if (separator == std::wstring::npos) {
            break;
        }
        start = separator + 1;
    }

    return parts;
}

bool TryParseModifier(const std::wstring &token, UINT *modifier) {
    static const std::unordered_map<std::wstring, UINT> kModifiers = {
        {L"ctrl", MOD_CONTROL},   {L"lctrl", MOD_CONTROL},
        {L"rctrl", MOD_CONTROL},  {L"shift", MOD_SHIFT},
        {L"lshift", MOD_SHIFT},   {L"rshift", MOD_SHIFT},
        {L"alt", MOD_ALT},        {L"lalt", MOD_ALT},
        {L"ralt", MOD_ALT},       {L"win", MOD_WIN},
        {L"lwin", MOD_WIN},       {L"rwin", MOD_WIN},
    };

    const auto found = kModifiers.find(token);
    if (found == kModifiers.end()) {
        return false;
    }

    *modifier = found->second;
    return true;
}

std::wstring NormalizeAlias(const std::wstring &token) {
    static const std::unordered_map<std::wstring, std::wstring> kAliases = {
        {L"esc", L"escape"},       {L"del", L"delete"},
        {L"ins", L"insert"},       {L"pgup", L"pageup"},
        {L"pgdn", L"pagedown"},    {L"return", L"enter"},
        {L"spacebar", L"space"},   {L"prtsc", L"printscreen"},
        {L"printscr", L"printscreen"},
        {L"menu", L"contextmenu"}, {L"apps", L"contextmenu"},
    };

    const auto found = kAliases.find(token);
    if (found == kAliases.end()) {
        return token;
    }

    return found->second;
}

bool TryParseFunctionKey(const std::wstring &token, UINT *virtualKey) {
    if (token.size() < 2 || token.front() != L'f') {
        return false;
    }

    int number = 0;
    for (size_t index = 1; index < token.size(); ++index) {
        if (!iswdigit(token[index])) {
            return false;
        }
        number = (number * 10) + (token[index] - L'0');
    }

    if (number < 1 || number > 24) {
        return false;
    }

    *virtualKey = VK_F1 + static_cast<UINT>(number - 1);
    return true;
}

bool TryParseKey(const std::wstring &token, UINT *virtualKey) {
    if (token.size() == 1) {
        const wchar_t character = token.front();
        if (character >= L'a' && character <= L'z') {
            *virtualKey = static_cast<UINT>(L'A' + (character - L'a'));
            return true;
        }

        if (character >= L'0' && character <= L'9') {
            *virtualKey = static_cast<UINT>(character);
            return true;
        }
    }

    if (TryParseFunctionKey(token, virtualKey)) {
        return true;
    }

    static const std::unordered_map<std::wstring, UINT> kKeys = {
        {L"space", VK_SPACE},
        {L"enter", VK_RETURN},
        {L"escape", VK_ESCAPE},
        {L"tab", VK_TAB},
        {L"backspace", VK_BACK},
        {L"insert", VK_INSERT},
        {L"delete", VK_DELETE},
        {L"home", VK_HOME},
        {L"end", VK_END},
        {L"pageup", VK_PRIOR},
        {L"pagedown", VK_NEXT},
        {L"up", VK_UP},
        {L"down", VK_DOWN},
        {L"left", VK_LEFT},
        {L"right", VK_RIGHT},
        {L"capslock", VK_CAPITAL},
        {L"numlock", VK_NUMLOCK},
        {L"scrolllock", VK_SCROLL},
        {L"printscreen", VK_SNAPSHOT},
        {L"pause", VK_PAUSE},
        {L"contextmenu", VK_APPS},
        {L"minus", VK_OEM_MINUS},
        {L"equal", VK_OEM_PLUS},
        {L"leftbracket", VK_OEM_4},
        {L"rightbracket", VK_OEM_6},
        {L"backslash", VK_OEM_5},
        {L"semicolon", VK_OEM_1},
        {L"quote", VK_OEM_7},
        {L"comma", VK_OEM_COMMA},
        {L"period", VK_OEM_PERIOD},
        {L"slash", VK_OEM_2},
        {L"backtick", VK_OEM_3},
        {L"numpad0", VK_NUMPAD0},
        {L"numpad1", VK_NUMPAD1},
        {L"numpad2", VK_NUMPAD2},
        {L"numpad3", VK_NUMPAD3},
        {L"numpad4", VK_NUMPAD4},
        {L"numpad5", VK_NUMPAD5},
        {L"numpad6", VK_NUMPAD6},
        {L"numpad7", VK_NUMPAD7},
        {L"numpad8", VK_NUMPAD8},
        {L"numpad9", VK_NUMPAD9},
        {L"numpadadd", VK_ADD},
        {L"numpadsubtract", VK_SUBTRACT},
        {L"numpadmultiply", VK_MULTIPLY},
        {L"numpaddivide", VK_DIVIDE},
        {L"numpaddecimal", VK_DECIMAL},
        // RegisterHotKey has no distinct VK_NUMPAD_ENTER. Distinguishing it
        // requires scan codes or hooks, so accept it as Enter.
        {L"numpadenter", VK_RETURN},
    };

    const auto found = kKeys.find(token);
    if (found == kKeys.end()) {
        return false;
    }

    *virtualKey = found->second;
    return true;
}

void SetError(std::wstring *errorMessage, const std::wstring &message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

bool ParseHotkey(const std::wstring &hotkeyText, ParsedHotkey *parsed,
                 std::wstring *errorMessage) {
    if (parsed == nullptr) {
        SetError(errorMessage, L"Internal error: missing output storage.");
        return false;
    }

    const std::vector<std::wstring> parts = SplitHotkeyText(hotkeyText);
    ParsedHotkey candidate;
    bool hasKey = false;

    for (const std::wstring &rawPart : parts) {
        if (rawPart.empty()) {
            SetError(errorMessage, L"Empty hotkey segment.");
            return false;
        }

        const std::wstring token = NormalizeAlias(ToLowerCopy(rawPart));

        UINT modifier = 0;
        if (TryParseModifier(token, &modifier)) {
            // RegisterHotKey only supports generic modifier flags, so
            // side-specific names like LCtrl and RCtrl map to MOD_CONTROL.
            if ((candidate.modifiers & modifier) != 0) {
                SetError(errorMessage, L"Duplicate modifier.");
                return false;
            }
            candidate.modifiers |= modifier;
            continue;
        }

        UINT virtualKey = 0;
        if (!TryParseKey(token, &virtualKey)) {
            SetError(errorMessage, L"Unknown key or modifier: " + rawPart);
            return false;
        }

        if (hasKey) {
            SetError(errorMessage, L"Hotkey must contain exactly one key.");
            return false;
        }

        candidate.virtualKey = virtualKey;
        hasKey = true;
    }

    if (!hasKey) {
        SetError(errorMessage, L"Hotkey must contain one key.");
        return false;
    }

    *parsed = candidate;
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

} // namespace clipass
