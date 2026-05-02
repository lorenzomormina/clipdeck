#pragma once

#include <string>

#include <windows.h>

namespace ClipDeck {

struct FocusSnapshot {
    HWND foregroundWindow = nullptr;
    HWND focusWindow = nullptr;
};

FocusSnapshot CaptureFocusedWindow();
bool CopyTextToClipboard(const std::wstring &text);
void PasteToWindow(const FocusSnapshot &snapshot);

} // namespace ClipDeck
