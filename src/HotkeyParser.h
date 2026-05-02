#pragma once

#include <string>

#include <windows.h>

namespace clipass {

struct ParsedHotkey {
    UINT modifiers = 0;
    UINT virtualKey = 0;
};

bool ParseHotkey(const std::wstring &hotkeyText, ParsedHotkey *parsed,
                 std::wstring *errorMessage);

} // namespace clipass
