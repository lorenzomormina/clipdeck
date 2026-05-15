#include "MainWindow.h"

#include "HotkeyParser.h"

#include <utility>

namespace ClipDeck {

MainWindow::MainWindow(HINSTANCE instance, const AppConfig &config)
    : instance_(instance), config_(config) {}

bool MainWindow::Create(int nCmdShow) {
    (void)nCmdShow;

    if (!CreateMainWindow()) {
        return false;
    }

    UpdateWindow(hwnd_);

    if (!trayIcon_.Add(hwnd_, instance_, kTrayCallbackMessage, L"ClipDeck")) {
        MessageBoxW(hwnd_, L"Could not load tray icon.", L"Error",
                    MB_ICONERROR);
    }
    RegisterGlobalHotkey();

    return true;
}

bool MainWindow::PreTranslateMessage(const MSG &message) {
    if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE && hwnd_ &&
        IsWindow(hwnd_) &&
        (message.hwnd == hwnd_ || IsChild(hwnd_, message.hwnd))) {
        HideWindow();
        return true;
    }

    return false;
}

void MainWindow::ApplyConfig() {
    if (!hwnd_) {
        return;
    }

    RegisterGlobalHotkey();

    clipListView_.SetMainWindowSettings(config_.mainWindowSettings);
    SetWindowPos(hwnd_, nullptr, 0, 0, config_.mainWindowSettings.width,
                 config_.mainWindowSettings.height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    RECT clientRect = {};
    GetClientRect(hwnd_, &clientRect);
    clipListView_.Layout(clientRect.right - clientRect.left,
                         clientRect.bottom - clientRect.top);
    clipListView_.SetGroups(config_.groups);
}

void MainWindow::SetOpenSettingsCallback(std::function<bool()> callback) {
    openSettingsCallback_ = std::move(callback);
}

void MainWindow::SetReloadConfigCallback(std::function<void()> callback) {
    reloadConfigCallback_ = std::move(callback);
}

void MainWindow::SetSettingsActiveCallback(std::function<bool()> callback) {
    isSettingsActiveCallback_ = std::move(callback);
}

bool MainWindow::CreateMainWindow() {
    if (!RegisterWindowClass()) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW, kWindowClassName, kWindowTitle,
        WS_OVERLAPPED | WS_SYSMENU | WS_SIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
        config_.mainWindowSettings.width, config_.mainWindowSettings.height,
        nullptr, nullptr, instance_, this);

    if (!hwnd_)
        return false;

    if (!config_.appSettings.startHidden) {
        ShowWindowAndActivate();
    }

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
    const std::wstring &hotkeyText = config_.hotkeySettings.open;
    hotkey_.Unregister();

    ParsedHotkey parsedHotkey;
    std::wstring parseError;
    if (!ParseHotkey(hotkeyText, &parsedHotkey, &parseError)) {
        const std::wstring message =
            L"Invalid global hotkey in settings.txt:\n\n\"" + hotkeyText +
            L"\"\n\n" + parseError;
        MessageBoxW(hwnd_, message.c_str(), L"Error", MB_ICONERROR);
        return false;
    }

    if (hotkey_.Register(hwnd_, kToggleHotkeyId, parsedHotkey.modifiers,
                         parsedHotkey.virtualKey)) {
        return true;
    }

    const std::wstring message =
        L"Failed to register global hotkey from settings.txt:\n\n\"" +
        hotkeyText + L"\"";
    MessageBoxW(hwnd_, message.c_str(), L"Error", MB_ICONERROR);
    return false;
}

void MainWindow::OpenSettingsWindow() {
    if (!openSettingsCallback_ || !openSettingsCallback_()) {
        MessageBoxW(hwnd_, L"Could not open settings window.", L"Error",
                    MB_ICONERROR);
        return;
    }

    if (IsWindowVisible(hwnd_) && config_.mainWindowSettings.hideOnBlur &&
        !config_.mainWindowSettings.keepVisibleWhileConfiguring) {
        HideWindow();
    }
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
    case WM_SIZE:
        clipListView_.Layout(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_ACTIVATE: {
        const bool settingsActive =
            isSettingsActiveCallback_ && isSettingsActiveCallback_();
        if (wParam == WA_INACTIVE && IsWindowVisible(hwnd_) &&
            config_.mainWindowSettings.hideOnBlur && !settingsActive &&
            !config_.mainWindowSettings.keepVisibleWhileConfiguring) {
            HideWindow();
            return 0;
        }
        break;
    }
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
    if (!clipListView_.Create(hwnd_, instance_, config_.mainWindowSettings)) {
        return -1;
    }

    clipListView_.SetGroups(config_.groups);
    return 0;
}

void MainWindow::OnDestroy() {
    hotkey_.Unregister();
    trayIcon_.Remove();
    clipListView_.Destroy();
    hwnd_ = nullptr;
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
        if (reloadConfigCallback_) {
            reloadConfigCallback_();
        }
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
        ActivateSelectedItem(false);
        return;
    }

    if (event == ClipListView::Event::ActivateSelectedItemCopyOnly) {
        ActivateSelectedItem(true);
        return;
    }

    if (event == ClipListView::Event::OpenSettings) {
        OpenSettingsWindow();
    }
}

void MainWindow::OnHotkey() {
    lastFocus_ = CaptureFocusedWindow();
    ToggleVisibility();
}

void MainWindow::ActivateSelectedItem(bool copyOnly) {
    const ClipItem *selectedItem = clipListView_.GetSelectedItem();
    if (!selectedItem) {
        return;
    }

    std::wstring activationText;
    std::wstring activationError;
    if (!TryGetActivationText(*selectedItem, &activationText,
                              &activationError)) {
        MessageBoxW(hwnd_, activationError.c_str(), L"ClipDeck",
                    MB_ICONERROR);
        return;
    }

    if (!CopyTextToClipboard(activationText)) {
        return;
    }

    if (copyOnly) {
        return;
    }

    if (selectedItem->autoClose) {
        HideWindow();
        if (selectedItem->autoPaste) {
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
    clipListView_.SelectFirstVisibleItem();
    clipListView_.FocusFilterBox();
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
} // namespace ClipDeck
