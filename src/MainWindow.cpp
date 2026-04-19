#include "MainWindow.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace clipass {

namespace {

constexpr int kSettingsTextAreaControlId = 3001;
constexpr int kSettingsSaveButtonControlId = 3002;
constexpr int kSettingsCancelButtonControlId = 3003;

HFONT CreateSystemUiFont() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                               &metrics, 0)) {
        return nullptr;
    }

    return CreateFontIndirectW(&metrics.lfMessageFont);
}

std::wstring NormalizeLineEndingsForEditor(const std::wstring &text) {
    std::wstring normalized;
    normalized.reserve(text.size());

    for (size_t index = 0; index < text.size(); ++index) {
        const wchar_t current = text[index];
        if (current == L'\r') {
            normalized.append(L"\r\n");
            if (index + 1 < text.size() && text[index + 1] == L'\n') {
                ++index;
            }
            continue;
        }

        if (current == L'\n') {
            normalized.append(L"\r\n");
            continue;
        }

        normalized.push_back(current);
    }

    return normalized;
}

std::wstring NormalizeLineEndingsForStorage(const std::wstring &text) {
    std::wstring normalized;
    normalized.reserve(text.size());

    for (size_t index = 0; index < text.size(); ++index) {
        const wchar_t current = text[index];
        if (current == L'\r') {
            normalized.push_back(L'\n');
            if (index + 1 < text.size() && text[index + 1] == L'\n') {
                ++index;
            }
            continue;
        }

        normalized.push_back(current);
    }

    return normalized;
}

std::wstring ReadTextFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return L"";
    }

    std::string bytes((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }

    if (bytes.empty()) {
        return L"";
    }

    const int utf8Chars =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                            static_cast<int>(bytes.size()), nullptr, 0);
    if (utf8Chars > 0) {
        std::wstring utf8Text(static_cast<size_t>(utf8Chars), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                            static_cast<int>(bytes.size()), utf8Text.data(),
                            utf8Chars);
        return utf8Text;
    }

    // Fallback for legacy ANSI files.
    const int acpChars = MultiByteToWideChar(
        CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (acpChars <= 0) {
        return L"";
    }

    std::wstring acpText(static_cast<size_t>(acpChars), L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()),
                        acpText.data(), acpChars);
    return acpText;
}

bool WriteTextFile(const std::filesystem::path &path,
                   const std::wstring &text) {
    const int bytesRequired = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                                  static_cast<int>(text.size()),
                                                  nullptr, 0, nullptr, nullptr);
    if (bytesRequired < 0) {
        return false;
    }

    std::string utf8Text(static_cast<size_t>(bytesRequired), '\0');
    if (bytesRequired > 0) {
        const int writtenBytes = WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            utf8Text.data(), bytesRequired, nullptr, nullptr);
        if (writtenBytes != bytesRequired) {
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    if (!utf8Text.empty()) {
        file.write(utf8Text.data(),
                   static_cast<std::streamsize>(utf8Text.size()));
    }

    return file.good();
}

} // namespace

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

void MainWindow::SetView(AppView view) {
    if (currentView_ == view) {
        return;
    }

    if (view == AppView::SETTINGS) {
        EnterSettingsView();
        return;
    }

    if (currentView_ == AppView::SETTINGS && view == AppView::MAIN_LIST) {
        TryLeaveSettingsView();
        return;
    }

    currentView_ = view;
    UpdateView();
}

AppView MainWindow::GetView() const { return currentView_; }

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

MainWindow::SettingsControls MainWindow::CreateSettingsControls() {
    SettingsControls controls;

    controls.textArea = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
            ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
        10, 10, 360, 220, hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSettingsTextAreaControlId)),
        instance_, nullptr);

    controls.saveButton = CreateWindowExW(
        0, L"BUTTON", L"Save", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 10, 240,
        90, 28, hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSettingsSaveButtonControlId)),
        instance_, nullptr);

    controls.cancelButton = CreateWindowExW(
        0, L"BUTTON", L"Cancel", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 110,
        240, 90, 28, hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSettingsCancelButtonControlId)),
        instance_, nullptr);

    if (!settingsUiFont_) {
        settingsUiFont_ = CreateSystemUiFont();
    }

    if (settingsUiFont_) {
        SendMessageW(controls.textArea, WM_SETFONT,
                     reinterpret_cast<WPARAM>(settingsUiFont_), TRUE);
        SendMessageW(controls.saveButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(settingsUiFont_), TRUE);
        SendMessageW(controls.cancelButton, WM_SETFONT,
                     reinterpret_cast<WPARAM>(settingsUiFont_), TRUE);
    }

    return controls;
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

void MainWindow::EnterSettingsView() {
    settingsState_.originalText =
        NormalizeLineEndingsForStorage(ReadTextFile(config_.configPath));
    settingsState_.isDirty = false;
    SetSettingsEditorText(settingsState_.originalText);
    currentView_ = AppView::SETTINGS;
    UpdateView();
}

void MainWindow::LeaveSettingsView() {
    currentView_ = AppView::MAIN_LIST;
    UpdateView();
}

bool MainWindow::TryLeaveSettingsView() {
    if (currentView_ != AppView::SETTINGS) {
        return true;
    }

    if (!settingsState_.isDirty) {
        LeaveSettingsView();
        return true;
    }

    const int result = MessageBoxW(
        hwnd_, L"You have unsaved changes. Save before leaving settings?",
        L"Unsaved changes", MB_YESNOCANCEL | MB_ICONWARNING);

    if (result == IDYES) {
        if (!SaveSettingsText()) {
            return false;
        }

        LeaveSettingsView();
        return true;
    }

    if (result == IDNO) {
        DiscardSettingsChanges();
        LeaveSettingsView();
        return true;
    }

    return false;
}

bool MainWindow::SaveSettingsText() {
    if (!hTextArea_) {
        return false;
    }

    const std::wstring currentText = ReadSettingsEditorText();
    if (!WriteTextFile(config_.configPath, currentText)) {
        MessageBoxW(hwnd_, L"Could not save configuration file.", L"Error",
                    MB_ICONERROR);
        return false;
    }

    settingsState_.originalText = currentText;
    settingsState_.isDirty = false;
    ReloadConfig();
    return true;
}

void MainWindow::DiscardSettingsChanges() {
    SetSettingsEditorText(settingsState_.originalText);
    settingsState_.isDirty = false;
}

bool MainWindow::HandleSettingsCommand(WPARAM wParam, LPARAM lParam) {
    const int controlId = LOWORD(wParam);
    const int notifyCode = HIWORD(wParam);
    const HWND controlHandle = reinterpret_cast<HWND>(lParam);

    if (controlId == kSettingsTextAreaControlId &&
        controlHandle == hTextArea_ && notifyCode == EN_CHANGE) {
        OnSettingsEditChanged();
        return true;
    }

    if (controlId == kSettingsSaveButtonControlId &&
        controlHandle == hSaveBtn_ && notifyCode == BN_CLICKED) {
        SaveSettingsText();
        return true;
    }

    if (controlId == kSettingsCancelButtonControlId &&
        controlHandle == hCancelBtn_ && notifyCode == BN_CLICKED) {
        TryLeaveSettingsView();
        return true;
    }

    return false;
}

void MainWindow::OnSettingsEditChanged() {
    if (settingsState_.suppressEditChange) {
        return;
    }

    settingsState_.isDirty =
        ReadSettingsEditorText() != settingsState_.originalText;
}

std::wstring MainWindow::ReadSettingsEditorText() const {
    if (!hTextArea_) {
        return L"";
    }

    const int textLength = GetWindowTextLengthW(hTextArea_);
    if (textLength <= 0) {
        return L"";
    }

    std::wstring text(static_cast<size_t>(textLength) + 1, L'\0');
    GetWindowTextW(hTextArea_, text.data(), textLength + 1);
    text.resize(static_cast<size_t>(textLength));
    return NormalizeLineEndingsForStorage(text);
}

void MainWindow::SetSettingsEditorText(const std::wstring &text) {
    if (!hTextArea_) {
        return;
    }

    settingsState_.suppressEditChange = true;
    const std::wstring editorText = NormalizeLineEndingsForEditor(text);
    SetWindowTextW(hTextArea_, editorText.c_str());
    settingsState_.suppressEditChange = false;
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        return OnCreate();
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (currentView_ == AppView::SETTINGS && !TryLeaveSettingsView()) {
                return 0;
            }
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
        if (currentView_ == AppView::SETTINGS && !TryLeaveSettingsView()) {
            return 0;
        }
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

    const SettingsControls settingsControls = CreateSettingsControls();
    hTextArea_ = settingsControls.textArea;
    hSaveBtn_ = settingsControls.saveButton;
    hCancelBtn_ = settingsControls.cancelButton;

    if (!hTextArea_ || !hSaveBtn_ || !hCancelBtn_) {
        return -1;
    }

    clipListView_.SetItems(config_.items);
    settingsState_.originalText = ReadSettingsEditorText();
    settingsState_.isDirty = false;
    UpdateView();
    return 0;
}

void MainWindow::OnDestroy() {
    hotkey_.Unregister();
    trayIcon_.Remove();
    clipListView_.Destroy();
    if (settingsUiFont_) {
        DeleteObject(settingsUiFont_);
        settingsUiFont_ = nullptr;
    }
    hwnd_ = nullptr;
}

void MainWindow::OnActivate(WPARAM wParam) {
    if (wParam == WA_INACTIVE && IsWindowVisible(hwnd_) &&
        currentView_ == AppView::MAIN_LIST) {
        HideWindow();
    }
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    if (HandleSettingsCommand(wParam, lParam)) {
        return;
    }

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
        return;
    }

    if (event == ClipListView::Event::OpenSettings) {
        SetView(AppView::SETTINGS);
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

void MainWindow::UpdateView() {
    const bool isMainListView = currentView_ == AppView::MAIN_LIST;
    const int mainViewCommand = isMainListView ? SW_SHOW : SW_HIDE;
    const int settingsViewCommand = isMainListView ? SW_HIDE : SW_SHOW;
    UpdateTaskbarVisibilityForCurrentView();

    const HWND hList = clipListView_.GetListHandle();
    const HWND hSearch = clipListView_.GetSearchHandle();
    const HWND hSettingsButton = clipListView_.GetSettingsButtonHandle();

    if (hList) {
        ShowWindow(hList, mainViewCommand);
    }

    if (hSearch) {
        ShowWindow(hSearch, mainViewCommand);
    }

    if (hSettingsButton) {
        ShowWindow(hSettingsButton, mainViewCommand);
    }

    if (hTextArea_) {
        ShowWindow(hTextArea_, settingsViewCommand);
    }

    if (hSaveBtn_) {
        ShowWindow(hSaveBtn_, settingsViewCommand);
    }

    if (hCancelBtn_) {
        ShowWindow(hCancelBtn_, settingsViewCommand);
    }
}

void MainWindow::UpdateTaskbarVisibilityForCurrentView() {
    if (!hwnd_) {
        return;
    }

    const bool showInTaskbar = currentView_ == AppView::SETTINGS;
    const LONG_PTR currentExStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    LONG_PTR updatedExStyle = currentExStyle;

    if (showInTaskbar) {
        updatedExStyle &= ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
        updatedExStyle |= static_cast<LONG_PTR>(WS_EX_APPWINDOW);
    } else {
        updatedExStyle &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
        updatedExStyle |= static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
    }

    if (updatedExStyle == currentExStyle) {
        return;
    }

    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, updatedExStyle);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);

    if (IsWindowVisible(hwnd_)) {
        ShowWindow(hwnd_, SW_HIDE);
        ShowWindow(hwnd_, SW_SHOW);
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
