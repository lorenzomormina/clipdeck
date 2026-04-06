#include "MainWindow.h"

#include <utility>

namespace clipass {

MainWindow::MainWindow(HINSTANCE instance, AppConfig config)
    : instance_(instance), config_(std::move(config)) {}

int MainWindow::Run(int nCmdShow) {
    (void)nCmdShow;

    if (!CreateMainWindow()) {
        return 0;
    }

    UpdateWindow(hwnd_);

    if (!trayIcon_.Add(hwnd_, kTrayIconId, kTrayCallbackMessage,
                       config_.iconPath, L"clipass")) {
        MessageBoxW(hwnd_, L"Could not load tray icon.", L"Error",
                    MB_ICONERROR);
    }
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
                            WS_OVERLAPPED | WS_SYSMENU, CW_USEDEFAULT,
                            CW_USEDEFAULT, config_.windowSettings.width,
                            config_.windowSettings.height, nullptr, nullptr,
                            instance_, this);

    if (!hwnd_)
        return false;

    // Show or hide window based on config_.settings.startHidden
    ShowWindow(hwnd_, config_.generalSettings.startHidden ? SW_HIDE : SW_SHOW);

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

bool MainWindow::RegisterGlobalHotkey() {
    if (hotkey_.Register(hwnd_, kToggleHotkeyId, MOD_CONTROL | MOD_SHIFT,
                         VK_OEM_3)) {
        return true;
    }

    MessageBoxW(hwnd_, L"Failed to register global hotkey (CTRL+SHIFT+GRAVE)",
                L"Error", MB_ICONERROR);
    return false;
}

void MainWindow::ReloadConfig() {
    config_ = LoadAppConfig();

    if (!hwnd_) {
        return;
    }

    SetWindowPos(hwnd_, nullptr, 0, 0, config_.windowSettings.width,
                 config_.windowSettings.height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    clipListView_.SetItems(config_.items);
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
        OnActivate(wParam);
        return 0;
    case WM_HOTKEY:
        if (hotkey_.Matches(wParam)) {
            OnHotkey();
            return 0;
        }
        break;
    case kTrayCallbackMessage:
        OnTrayCallback(lParam);
        return 0;
    case WM_COMMAND:
        OnCommand(wParam, lParam);
        return 0;
    case WM_CLOSE:
        HideWindow();
        return 0;
    case WM_TIMER:
        if (clipListView_.HandleTimer(hwnd_, wParam)) {
            return 0;
        }
        break;
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
    if (!clipListView_.Create(hwnd_, instance_, config_.windowSettings)) {
        return -1;
    }

    clipListView_.SetItems(config_.items);
    return 0;
}

void MainWindow::OnDestroy() {
    hotkey_.Unregister();
    trayIcon_.Remove();
    clipListView_.Destroy();
    hwnd_ = nullptr;
}

void MainWindow::OnActivate(WPARAM wParam) {
    if (wParam == WA_INACTIVE && IsWindowVisible(hwnd_)) {
        HideWindow();
    }
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    OnClipListEvent(clipListView_.HandleCommand(hwnd_, wParam, lParam));
}

void MainWindow::OnTrayCallback(LPARAM lParam) {
    switch (trayIcon_.TranslateCallback(lParam)) {
    case TrayIcon::CallbackAction::OpenWindow:
        ShowWindowAndActivate();
        break;
    case TrayIcon::CallbackAction::ShowMenu:
        OnTrayMenuAction(trayIcon_.ShowContextMenu(hwnd_));
        break;
    case TrayIcon::CallbackAction::None:
        break;
    }
}

void MainWindow::OnTrayMenuAction(TrayIcon::MenuAction action) {
    switch (action) {
    case TrayIcon::MenuAction::OpenWindow:
        ShowWindowAndActivate();
        break;
    case TrayIcon::MenuAction::ReloadConfig:
        ReloadConfig();
        break;
    case TrayIcon::MenuAction::ShowConfig:
        MessageBoxW(hwnd_, L"Config option not implemented yet.", L"Info",
                    MB_ICONINFORMATION);
        break;
    case TrayIcon::MenuAction::ExitApp:
        DestroyWindow(hwnd_);
        break;
    case TrayIcon::MenuAction::None:
        break;
    }
}

void MainWindow::OnClipListEvent(ClipListView::Event event) {
    if (event == ClipListView::Event::ActivateSelectedItem) {
        ActivateSelectedItem();
    }
}

void MainWindow::OnHotkey() {
    lastFocus_ = CaptureFocusedWindow();
    ToggleVisibility();
}

void MainWindow::ActivateSelectedItem() {
    const auto selectedIndex = clipListView_.GetSelectedItemIndex();
    if (!selectedIndex || *selectedIndex >= config_.items.size()) {
        return;
    }

    const std::wstring &selectedValue = config_.items[*selectedIndex].value;
    if (!CopyTextToClipboard(selectedValue)) {
        return;
    }

    if (config_.generalSettings.autoClose) {
        HideWindow();
        if (config_.generalSettings.autoPaste) {
            PasteToWindow(lastFocus_);
        }
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

LRESULT CALLBACK MainWindow::WindowProcSetup(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
    if (message != WM_NCCREATE) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto *createStruct = reinterpret_cast<const CREATESTRUCTW *>(lParam);
    auto *self = static_cast<MainWindow *>(createStruct->lpCreateParams);
    self->hwnd_ = hwnd;

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(&MainWindow::WindowProcThunk));

    return TRUE;
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
} // namespace clipass
