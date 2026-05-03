#include "SettingsWindow.h"
#include "../resources/resource.h"

#include <algorithm>
#include <commctrl.h>
#include <fstream>
#include <utility>

namespace ClipDeck {

namespace {

constexpr int kSettingsTextAreaControlId = 3001;
constexpr int kSettingsSaveButtonControlId = 3002;
constexpr int kSettingsCancelButtonControlId = 3003;
constexpr int kIconButtonSize = 20;
constexpr int kIconImageSize = 16;
wchar_t kSaveToolTipText[] = L"Save";
wchar_t kCancelToolTipText[] = L"Cancel";

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

SettingsWindow::SettingsWindow(HINSTANCE instance) : instance_(instance) {}

SettingsWindow::~SettingsWindow() { Destroy(); }

void SettingsWindow::SetConfig(const AppConfig &config) {
    configPath_ = config.configPath;
    configWindowSettings_ = config.configWindowSettings;
    // ApplyWindowSize();
    LayoutControls();
}

void SettingsWindow::SetConfigSavedCallback(std::function<void()> callback) {
    onConfigSaved_ = std::move(callback);
}

bool SettingsWindow::Show() {
    if (!hwnd_ && !CreateWindowInstance()) {
        return false;
    }

    if (!IsWindowVisible(hwnd_)) {
        LoadTextFromDisk();
        ShowWindow(hwnd_, SW_SHOW);
    } else {
        ShowWindow(hwnd_, SW_SHOW);
    }

    SetForegroundWindow(hwnd_);
    if (hTextArea_) {
        SetFocus(hTextArea_);
    }

    return true;
}

void SettingsWindow::Destroy() {
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        return;
    }

    if (uiFont_) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
    }
}

bool SettingsWindow::PreTranslateMessage(const MSG &message) {
    if (message.message != WM_KEYDOWN || message.wParam != VK_ESCAPE ||
        !hwnd_ || !IsWindow(hwnd_)) {
        return false;
    }

    if (message.hwnd != hwnd_ && !IsChild(hwnd_, message.hwnd)) {
        return false;
    }

    TryCloseOrHide();
    return true;
}

bool SettingsWindow::RegisterWindowClass() const {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &SettingsWindow::WindowProcSetup;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    const ATOM atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool SettingsWindow::CreateWindowInstance() {
    if (!RegisterWindowClass()) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0, kWindowClassName, kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, CW_USEDEFAULT,
        CW_USEDEFAULT, configWindowSettings_.width,
        configWindowSettings_.height, nullptr, nullptr, instance_, this);
    return hwnd_ != nullptr;
}

bool SettingsWindow::CreateControls() {
    hTextArea_ =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN |
                            WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
                        0, 0, 0, 0, hwnd_,
                        reinterpret_cast<HMENU>(
                            static_cast<INT_PTR>(kSettingsTextAreaControlId)),
                        instance_, nullptr);

    hSaveBtn_ = CreateWindowExW(
        0, L"BUTTON", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_ICON, 0, 0,
        kIconButtonSize, kIconButtonSize, hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSettingsSaveButtonControlId)),
        instance_, nullptr);

    hCancelBtn_ = CreateWindowExW(
        0, L"BUTTON", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_ICON, 0, 0,
        kIconButtonSize, kIconButtonSize, hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSettingsCancelButtonControlId)),
        instance_, nullptr);

    if (!hTextArea_ || !hSaveBtn_ || !hCancelBtn_) {
        return false;
    }

    if (!uiFont_) {
        uiFont_ = CreateSystemUiFont();
    }

    if (uiFont_) {
        SendMessageW(hTextArea_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_),
                     TRUE);
        SendMessageW(hSaveBtn_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_),
                     TRUE);
        SendMessageW(hCancelBtn_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_),
                     TRUE);
    }

    const int iconSize = kIconImageSize;
    hSaveIcon_ = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_SAVE), IMAGE_ICON, iconSize,
                   iconSize, LR_DEFAULTCOLOR));
    hCancelIcon_ = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_CANCEL), IMAGE_ICON,
                   iconSize, iconSize, LR_DEFAULTCOLOR));
    if (!hSaveIcon_ || !hCancelIcon_) {
        return false;
    }

    SendMessageW(hSaveBtn_, BM_SETIMAGE, IMAGE_ICON,
                 reinterpret_cast<LPARAM>(hSaveIcon_));
    SendMessageW(hCancelBtn_, BM_SETIMAGE, IMAGE_ICON,
                 reinterpret_cast<LPARAM>(hCancelIcon_));

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&commonControls)) {
        return true;
    }

    hSaveToolTip_ = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, instance_, nullptr);
    if (hSaveToolTip_) {
        TOOLINFOW toolInfo = {};
#ifdef TTTOOLINFOW_V2_SIZE
        toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
#else
        toolInfo.cbSize = sizeof(toolInfo);
#endif
        toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        toolInfo.hwnd = hwnd_;
        toolInfo.uId = reinterpret_cast<UINT_PTR>(hSaveBtn_);
        toolInfo.lpszText = kSaveToolTipText;

        if (!SendMessageW(hSaveToolTip_, TTM_ADDTOOLW, 0,
                          reinterpret_cast<LPARAM>(&toolInfo))) {
#ifdef TTTOOLINFOW_V1_SIZE
            toolInfo.cbSize = TTTOOLINFOW_V1_SIZE;
            if (!SendMessageW(hSaveToolTip_, TTM_ADDTOOLW, 0,
                              reinterpret_cast<LPARAM>(&toolInfo))) {
                DestroyWindow(hSaveToolTip_);
                hSaveToolTip_ = nullptr;
            }
#else
            DestroyWindow(hSaveToolTip_);
            hSaveToolTip_ = nullptr;
#endif
        }
    }

    hCancelToolTip_ = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, instance_, nullptr);
    if (hCancelToolTip_) {
        TOOLINFOW toolInfo = {};
#ifdef TTTOOLINFOW_V2_SIZE
        toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
#else
        toolInfo.cbSize = sizeof(toolInfo);
#endif
        toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        toolInfo.hwnd = hwnd_;
        toolInfo.uId = reinterpret_cast<UINT_PTR>(hCancelBtn_);
        toolInfo.lpszText = kCancelToolTipText;

        if (!SendMessageW(hCancelToolTip_, TTM_ADDTOOLW, 0,
                          reinterpret_cast<LPARAM>(&toolInfo))) {
#ifdef TTTOOLINFOW_V1_SIZE
            toolInfo.cbSize = TTTOOLINFOW_V1_SIZE;
            if (!SendMessageW(hCancelToolTip_, TTM_ADDTOOLW, 0,
                              reinterpret_cast<LPARAM>(&toolInfo))) {
                DestroyWindow(hCancelToolTip_);
                hCancelToolTip_ = nullptr;
            }
#else
            DestroyWindow(hCancelToolTip_);
            hCancelToolTip_ = nullptr;
#endif
        }
    }

    return true;
}

// si triggera quando cambia la dimensione della finestra; se cambia solo il
// margine nella config, non viene chiamata questa funzione
void SettingsWindow::LayoutControls() const {
    if (!hwnd_ || !hTextArea_ || !hSaveBtn_ || !hCancelBtn_) {
        return;
    }

    SetWindowPos(hwnd_, nullptr, 0, 0, configWindowSettings_.width,
                 configWindowSettings_.height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    RECT clientRect{};
    if (!GetClientRect(hwnd_, &clientRect)) {
        return;
    }

    const int margin = configWindowSettings_.margin;
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    const int buttonsTop = margin;
    const int buttonsLeft = margin;
    const int textTop = buttonsTop + kIconButtonSize + margin;
    const int textWidth = std::max(0, clientWidth - (margin * 2));
    const int textHeight = std::max(0, clientHeight - textTop - margin);
    MoveWindow(hSaveBtn_, buttonsLeft, buttonsTop, kIconButtonSize,
               kIconButtonSize, TRUE);
    MoveWindow(hCancelBtn_, buttonsLeft + kIconButtonSize + margin, buttonsTop,
               kIconButtonSize, kIconButtonSize, TRUE);
    MoveWindow(hTextArea_, margin, textTop, textWidth, textHeight, TRUE);
}

void SettingsWindow::LoadTextFromDisk() {
    settingsState_.originalText =
        NormalizeLineEndingsForStorage(ReadTextFile(configPath_));
    settingsState_.isDirty = false;
    SetEditorText(settingsState_.originalText);
}

void SettingsWindow::Hide() {
    if (!hwnd_) {
        return;
    }

    ShowWindow(hwnd_, SW_HIDE);
}

bool SettingsWindow::TryCloseOrHide() {
    if (!settingsState_.isDirty) {
        Hide();
        return true;
    }

    const int result = MessageBoxW(
        hwnd_, L"You have unsaved changes. Save before leaving settings?",
        L"Unsaved changes", MB_YESNOCANCEL | MB_ICONWARNING);

    if (result == IDYES) {
        if (!SaveText()) {
            return false;
        }

        Hide();
        return true;
    }

    if (result == IDNO) {
        DiscardChanges();
        Hide();
        return true;
    }

    return false;
}

bool SettingsWindow::SaveText() {
    if (!hTextArea_) {
        return false;
    }

    const std::wstring currentText = ReadEditorText();
    if (!WriteTextFile(configPath_, currentText)) {
        MessageBoxW(hwnd_, L"Could not save configuration file.", L"Error",
                    MB_ICONERROR);
        return false;
    }

    settingsState_.originalText = currentText;
    settingsState_.isDirty = false;

    if (onConfigSaved_) {
        onConfigSaved_();
    }

    return true;
}

void SettingsWindow::DiscardChanges() {
    SetEditorText(settingsState_.originalText);
    settingsState_.isDirty = false;
}

bool SettingsWindow::HandleCommand(WPARAM wParam, LPARAM lParam) {
    const int controlId = LOWORD(wParam);
    const int notifyCode = HIWORD(wParam);
    const HWND controlHandle = reinterpret_cast<HWND>(lParam);

    if (controlId == kSettingsTextAreaControlId &&
        controlHandle == hTextArea_ && notifyCode == EN_CHANGE) {
        OnEditChanged();
        return true;
    }

    if (controlId == kSettingsSaveButtonControlId &&
        controlHandle == hSaveBtn_ && notifyCode == BN_CLICKED) {
        SaveText();
        return true;
    }

    if (controlId == kSettingsCancelButtonControlId &&
        controlHandle == hCancelBtn_ && notifyCode == BN_CLICKED) {
        TryCloseOrHide();
        return true;
    }

    return false;
}

void SettingsWindow::OnEditChanged() {
    if (settingsState_.suppressEditChange) {
        return;
    }

    settingsState_.isDirty = ReadEditorText() != settingsState_.originalText;
}

std::wstring SettingsWindow::ReadEditorText() const {
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

void SettingsWindow::SetEditorText(const std::wstring &text) {
    if (!hTextArea_) {
        return;
    }

    settingsState_.suppressEditChange = true;
    const std::wstring editorText = NormalizeLineEndingsForEditor(text);
    SetWindowTextW(hTextArea_, editorText.c_str());
    settingsState_.suppressEditChange = false;
}

LRESULT SettingsWindow::HandleMessage(UINT message, WPARAM wParam,
                                      LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (!CreateControls()) {
            return -1;
        }
        LayoutControls();
        return 0;
    case WM_SIZE:
        LayoutControls();
        return 0;
    case WM_COMMAND:
        if (HandleCommand(wParam, lParam)) {
            return 0;
        }
        break;
    case WM_CLOSE:
        TryCloseOrHide();
        return 0;
    case WM_DESTROY:
        hTextArea_ = nullptr;
        hSaveBtn_ = nullptr;
        hCancelBtn_ = nullptr;
        if (hSaveToolTip_ && IsWindow(hSaveToolTip_)) {
            DestroyWindow(hSaveToolTip_);
            hSaveToolTip_ = nullptr;
        }
        if (hCancelToolTip_ && IsWindow(hCancelToolTip_)) {
            DestroyWindow(hCancelToolTip_);
            hCancelToolTip_ = nullptr;
        }
        if (hSaveIcon_) {
            DestroyIcon(hSaveIcon_);
            hSaveIcon_ = nullptr;
        }
        if (hCancelIcon_) {
            DestroyIcon(hCancelIcon_);
            hCancelIcon_ = nullptr;
        }
        settingsState_ = {};
        if (uiFont_) {
            DeleteObject(uiFont_);
            uiFont_ = nullptr;
        }
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::WindowProcSetup(HWND hwnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam) {
    if (message != WM_NCCREATE) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const auto *createStruct = reinterpret_cast<const CREATESTRUCTW *>(lParam);
    auto *self = static_cast<SettingsWindow *>(createStruct->lpCreateParams);
    self->hwnd_ = hwnd;

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&SettingsWindow::WindowProcThunk));

    return TRUE;
}

LRESULT CALLBACK SettingsWindow::WindowProcThunk(HWND hwnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam) {
    auto *self = reinterpret_cast<SettingsWindow *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return self->HandleMessage(message, wParam, lParam);
}

} // namespace ClipDeck
