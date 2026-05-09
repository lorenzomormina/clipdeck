#include "TrayIcon.h"

#include "resource.h"
#include <shellapi.h>

namespace ClipDeck {

TrayIcon::~TrayIcon() { Remove(); }

bool TrayIcon::Add(HWND owner, HINSTANCE instance, UINT callbackMessage,
                   const wchar_t *toolTip) {
    Remove();

    const int iconSize = 16;

    owner_ = owner;
    iconId_ = IDI_ICON1;
    icon_ = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(iconId_),
                                          IMAGE_ICON, iconSize, iconSize,
                                          LR_DEFAULTCOLOR));
    if (!icon_) {
        return false;
    }

    NOTIFYICONDATAW trayData = {};
    trayData.cbSize = sizeof(trayData);
    trayData.hWnd = owner_;
    trayData.uID = iconId_;
    trayData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    trayData.uCallbackMessage = callbackMessage;
    trayData.hIcon = icon_;
    wcscpy_s(trayData.szTip, toolTip);

    isAdded_ = Shell_NotifyIconW(NIM_ADD, &trayData) == TRUE;
    if (isAdded_) {
        return true;
    }

    DestroyIcon(icon_);
    icon_ = nullptr;
    owner_ = nullptr;
    iconId_ = 0;
    return false;
}

void TrayIcon::Remove() {
    if (isAdded_ && owner_) {
        NOTIFYICONDATAW trayData = {};
        trayData.cbSize = sizeof(trayData);
        trayData.hWnd = owner_;
        trayData.uID = iconId_;
        Shell_NotifyIconW(NIM_DELETE, &trayData);
    }

    if (icon_) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }

    owner_ = nullptr;
    iconId_ = 0;
    isAdded_ = false;
}

TrayIcon::CallbackAction TrayIcon::TranslateCallback(LPARAM lParam) const {
    if (lParam == WM_LBUTTONUP) {
        return CallbackAction::OpenWindow;
    }

    if (lParam == WM_RBUTTONUP) {
        return CallbackAction::ShowMenu;
    }

    return CallbackAction::None;
}

TrayIcon::MenuAction TrayIcon::ShowContextMenu(HWND owner) const {
    if (!owner) {
        return MenuAction::None;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return MenuAction::None;
    }

    InsertMenuW(menu, -1, MF_BYPOSITION, kOpenCommandId, L"Open");
    InsertMenuW(menu, -1, MF_BYPOSITION, kConfigCommandId, L"Config");
    InsertMenuW(menu, -1, MF_BYPOSITION, kReloadCommandId, L"Reload");
    InsertMenuW(menu, -1, MF_BYPOSITION, kExitCommandId, L"Exit");

    POINT cursor = {};
    GetCursorPos(&cursor);

    SetForegroundWindow(owner);
    const UINT command =
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                       cursor.x, cursor.y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (command) {
    case kOpenCommandId:
        return MenuAction::OpenWindow;
    case kReloadCommandId:
        return MenuAction::ReloadConfig;
    case kConfigCommandId:
        return MenuAction::ShowConfig;
    case kExitCommandId:
        return MenuAction::ExitApp;
    default:
        return MenuAction::None;
    }
}

} // namespace ClipDeck
