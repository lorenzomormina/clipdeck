#define UNICODE

#include "MainWindow.h"

#include <iterator>
#include <string>
#include <utility>

#include <shellapi.h>

namespace clipass {

MainWindow::MainWindow(HINSTANCE instance, AppConfig config)
    : instance_(instance), config_(std::move(config)) {}

MainWindow::~MainWindow() { ReleaseUiResources(); }

int MainWindow::Run(int nCmdShow) {
    if (!CreateMainWindow()) {
        return 0;
    }

    UpdateWindow(hwnd_);

    AddTrayIcon();
    RegisterGlobalHotkey();

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

bool MainWindow::CreateMainWindow() {
    if (!RegisterWindowClass()) {
        return false;
    }

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClassName, kWindowTitle,
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            400, 300, nullptr, nullptr, instance_, this);

    if (!hwnd_)
        return false;

    // Show or hide window based on config_.settings.startHidden
    ShowWindow(hwnd_, config_.settings.startHidden ? SW_HIDE : SW_SHOW);

    return true;
}

bool MainWindow::RegisterWindowClass() const {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &MainWindow::WindowProcSetup;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    const ATOM atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        return OnCreate();
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            HideWindow();
            return 0;
        }
        break;
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE && IsWindowVisible(hwnd_)) {
            HideWindow();
        }
        break;
    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            OnHotkey();
            return 0;
        }
        break;
    case kTrayCallbackMessage:
        OnTrayIconMessage(lParam);
        return 0;
    case WM_COMMAND:
        OnCommand(wParam, lParam);
        return 0;
    case WM_CLOSE:
        HideWindow();
        return 0;
    case WM_DESTROY:
        OnDestroy();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT MainWindow::OnCreate() {
    listBox_ = CreateWindowExW(
        0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER, 10, 10,
        360, 240, hwnd_, nullptr, instance_, nullptr);
    if (!listBox_) {
        return -1;
    }

    uiFont_ = CreateSystemUiFont();
    if (uiFont_) {
        SendMessageW(listBox_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_),
                     TRUE);
    }

    PopulateListBox();

    SetWindowLongPtrW(listBox_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    originalListBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        listBox_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&MainWindow::ListBoxProcThunk)));

    return 0;
}

void MainWindow::OnDestroy() {
    RemoveTrayIcon();
    ReleaseUiResources();
    UnregisterHotKey(hwnd_, kHotkeyId);
    listBox_ = nullptr;
    hwnd_ = nullptr;
}

void MainWindow::OnHotkey() {
    lastFocus_ = CaptureFocusedWindow();
    ToggleVisibility();
}

void MainWindow::OnTrayIconMessage(LPARAM lParam) {
    if (lParam == WM_RBUTTONUP) {
        POINT cursor = {};
        GetCursorPos(&cursor);

        HMENU menu = CreatePopupMenu();
        InsertMenuW(menu, -1, MF_BYPOSITION, kTrayOpenCommandId, L"Open");
        InsertMenuW(menu, -1, MF_BYPOSITION, kTrayConfigCommandId, L"Config");
        InsertMenuW(menu, -1, MF_BYPOSITION, kTrayReloadCommandId, L"Reload");
        InsertMenuW(menu, -1, MF_BYPOSITION, kTrayExitCommandId, L"Exit");

        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_,
                       nullptr);
        DestroyMenu(menu);
        return;
    }

    if (lParam == WM_LBUTTONUP) {
        ShowWindowAndActivate();
    }
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    if (reinterpret_cast<HWND>(lParam) == listBox_ &&
        HIWORD(wParam) == LBN_DBLCLK) {
        ActivateSelectedItem();
        return;
    }

    switch (LOWORD(wParam)) {
    case kTrayOpenCommandId:
        ShowWindowAndActivate();
        break;
    case kTrayExitCommandId:
        DestroyWindow(hwnd_);
        break;
    case kTrayConfigCommandId:
        MessageBoxW(hwnd_, L"Config option not implemented yet.", L"Info",
                    MB_ICONINFORMATION);
        break;
    case kTrayReloadCommandId:
        config_ = clipass::LoadAppConfig();
        PopulateListBox();
        break;
    default:
        break;
    }
}

void MainWindow::ToggleVisibility() {
    if (IsWindowVisible(hwnd_)) {
        HideWindow();
        return;
    }

    ShowWindowAndActivate();
}

void MainWindow::ShowWindowAndActivate() {
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void MainWindow::HideWindow() { ShowWindow(hwnd_, SW_HIDE); }

void MainWindow::PopulateListBox() const {
    if (!listBox_) {
        return;
    }

    SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
    for (const auto &item : config_.items) {
        const std::wstring displayText = item.GetDisplayText();
        SendMessageW(listBox_, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(displayText.c_str()));
    }
}

void MainWindow::ActivateSelectedItem() {
    if (!listBox_) {
        return;
    }

    const int selectedIndex =
        static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
    if (selectedIndex == LB_ERR || selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(config_.items.size())) {
        return;
    }

    const std::wstring &selectedValue = config_.items[selectedIndex].value;
    if (!CopyTextToClipboard(selectedValue)) {
        return;
    }

    if (config_.settings.autoClose) {
        HideWindow();
    }
    if (config_.settings.autoPaste) {
        PasteToWindow(lastFocus_);
    }
}

bool MainWindow::RegisterGlobalHotkey() const {
    if (RegisterHotKey(hwnd_, kHotkeyId, MOD_CONTROL | MOD_SHIFT, VK_OEM_3)) {
        return true;
    }

    MessageBoxW(hwnd_, L"Failed to register global hotkey (CTRL+SHIFT+GRAVE)",
                L"Error", MB_ICONERROR);
    return false;
}

bool MainWindow::AddTrayIcon() {
    trayIcon_ = static_cast<HICON>(
        LoadImageW(nullptr, config_.iconPath.c_str(), IMAGE_ICON, 0, 0,
                   LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (!trayIcon_) {
        MessageBoxW(hwnd_, L"Could not load tray icon.", L"Error",
                    MB_ICONERROR);
        return false;
    }

    NOTIFYICONDATAW trayData = {};
    trayData.cbSize = sizeof(trayData);
    trayData.hWnd = hwnd_;
    trayData.uID = kTrayIconId;
    trayData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    trayData.uCallbackMessage = kTrayCallbackMessage;
    trayData.hIcon = trayIcon_;
    wcscpy_s(trayData.szTip, L"clipass");

    return Shell_NotifyIconW(NIM_ADD, &trayData) == TRUE;
}

void MainWindow::RemoveTrayIcon() {
    if (!hwnd_) {
        return;
    }

    NOTIFYICONDATAW trayData = {};
    trayData.cbSize = sizeof(trayData);
    trayData.hWnd = hwnd_;
    trayData.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &trayData);
}

void MainWindow::ReleaseUiResources() {
    if (listBox_ && IsWindow(listBox_) && originalListBoxProc_) {
        SetWindowLongPtrW(listBox_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalListBoxProc_));
    }
    originalListBoxProc_ = nullptr;

    if (trayIcon_) {
        DestroyIcon(trayIcon_);
        trayIcon_ = nullptr;
    }

    if (uiFont_) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
    }
}

HFONT MainWindow::CreateSystemUiFont() const {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                               &metrics, 0)) {
        return nullptr;
    }

    return CreateFontIndirectW(&metrics.lfMessageFont);
}

LRESULT CALLBACK MainWindow::WindowProcSetup(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto *createStruct =
            reinterpret_cast<const CREATESTRUCTW *>(lParam);
        auto *self = static_cast<MainWindow *>(createStruct->lpCreateParams);
        self->hwnd_ = hwnd;

        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&MainWindow::WindowProcThunk));

        return self->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::WindowProcThunk(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
    auto *self =
        reinterpret_cast<MainWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return self->HandleMessage(message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::ListBoxProcThunk(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam) {
    auto *self =
        reinterpret_cast<MainWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self || !self->originalListBoxProc_) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == WM_KEYDOWN && wParam == VK_RETURN) {
        self->ActivateSelectedItem();
        return 0;
    }

    return CallWindowProcW(self->originalListBoxProc_, hwnd, message, wParam,
                           lParam);
}

} // namespace clipass
