#include "ClipboardUtils.h"

#include <cstring>

namespace clipass {

FocusSnapshot CaptureFocusedWindow() {
    FocusSnapshot snapshot;
    snapshot.foregroundWindow = GetForegroundWindow();
    if (!snapshot.foregroundWindow) {
        return snapshot;
    }

    DWORD processId = 0;
    const DWORD threadId =
        GetWindowThreadProcessId(snapshot.foregroundWindow, &processId);

    GUITHREADINFO threadInfo = {};
    threadInfo.cbSize = sizeof(threadInfo);
    if (GetGUIThreadInfo(threadId, &threadInfo)) {
        snapshot.focusWindow = threadInfo.hwndFocus;
    }

    return snapshot;
}

bool CopyTextToClipboard(const std::wstring &text) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    EmptyClipboard();

    const size_t byteCount = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL clipboardData = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!clipboardData) {
        CloseClipboard();
        return false;
    }

    void *memory = GlobalLock(clipboardData);
    std::memcpy(memory, text.c_str(), byteCount);
    GlobalUnlock(clipboardData);

    if (!SetClipboardData(CF_UNICODETEXT, clipboardData)) {
        GlobalFree(clipboardData);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

void PasteToWindow(const FocusSnapshot &snapshot) {
    if (!snapshot.foregroundWindow || !IsWindow(snapshot.foregroundWindow)) {
        return;
    }

    SetForegroundWindow(snapshot.foregroundWindow);
    if (snapshot.focusWindow && IsWindow(snapshot.focusWindow)) {
        SetFocus(snapshot.focusWindow);
    }

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT));
}

} // namespace clipass
