#include "ClipListView.h"
#include "../resources/resource.h"

#include <algorithm>
#include <commctrl.h>
#include <string>

namespace ClipDeck {

namespace {
wchar_t kConfigurationToolTipText[] = L"Configuration";
}

ClipListView::~ClipListView() { Destroy(); }

bool ClipListView::Create(HWND parent, HINSTANCE instance,
                          WindowSettings settings) {
    parent_ = parent;
    windowSettings_ = settings;
    uiTextHeight_ = 0;

    listBox_ = CreateWindowExW(
        0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER |
            LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, parent_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListBoxControlId)),
        instance, nullptr);
    if (!listBox_) {
        Destroy();
        return false;
    }

    filterTextBox_ = CreateWindowExW(
        0, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 0, 0, parent_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFilterTextBoxControlId)),
        instance, nullptr);
    if (!filterTextBox_) {
        Destroy();
        return false;
    }

    settingsButton_ = CreateWindowExW(
        0, L"BUTTON", nullptr, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON,
        0, 0, 0, 0, parent_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsButtonControlId)),
        instance, nullptr);
    if (!settingsButton_) {
        Destroy();
        return false;
    }

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&commonControls)) {
        Destroy();
        return false;
    }

    settingsToolTip_ = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, parent_, nullptr, instance, nullptr);
    if (settingsToolTip_) {
        TOOLINFOW toolInfo = {};
#ifdef TTTOOLINFOW_V2_SIZE
        toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
#else
        toolInfo.cbSize = sizeof(toolInfo);
#endif
        toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        toolInfo.hwnd = parent_;
        toolInfo.uId = reinterpret_cast<UINT_PTR>(settingsButton_);
        toolInfo.lpszText = kConfigurationToolTipText;

        if (!SendMessageW(settingsToolTip_, TTM_ADDTOOLW, 0,
                          reinterpret_cast<LPARAM>(&toolInfo))) {
#ifdef TTTOOLINFOW_V1_SIZE
            toolInfo.cbSize = TTTOOLINFOW_V1_SIZE;
            if (!SendMessageW(settingsToolTip_, TTM_ADDTOOLW, 0,
                              reinterpret_cast<LPARAM>(&toolInfo))) {
                DestroyWindow(settingsToolTip_);
                settingsToolTip_ = nullptr;
            }
#else
            DestroyWindow(settingsToolTip_);
            settingsToolTip_ = nullptr;
#endif
        }
    }

    uiFont_ = CreateSystemUiFont();
    if (uiFont_) {
        SendMessageW(listBox_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_),
                     TRUE);
        SendMessageW(filterTextBox_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(uiFont_), TRUE);

        HFONT hFont = uiFont_;
        HDC hdc = GetDC(parent_);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        const int measuredTextHeight = tm.tmHeight;

        SelectObject(hdc, oldFont);
        ReleaseDC(parent_, hdc);
        uiTextHeight_ = measuredTextHeight;
    }

    const int iconSize = std::max(16, GetSystemMetrics(SM_CXSMICON));
    settingsIcon_ = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(IDI_SETTINGS), IMAGE_ICON,
                   iconSize, iconSize, LR_DEFAULTCOLOR));
    if (!settingsIcon_) {
        Destroy();
        return false;
    }

    SendMessageW(settingsButton_, BM_SETIMAGE, IMAGE_ICON,
                 reinterpret_cast<LPARAM>(settingsIcon_));

    RECT clientRect = {};
    GetClientRect(parent_, &clientRect);
    Layout(clientRect.right - clientRect.left,
           clientRect.bottom - clientRect.top);

    SetWindowLongPtrW(listBox_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    originalListBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        listBox_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&ClipListView::ListBoxProcThunk)));

    SetWindowLongPtrW(filterTextBox_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    originalFilterTextBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        filterTextBox_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&ClipListView::FilterTextBoxProcThunk)));

    return true;
}

void ClipListView::SetWindowSettings(WindowSettings settings) {
    windowSettings_ = settings;
}

void ClipListView::Layout(int clientWidth, int clientHeight) {
    if (!parent_ || !listBox_ || !filterTextBox_ || !settingsButton_) {
        return;
    }

    const int margin = windowSettings_.margin;
    const int originalTextBoxWidth = std::max(0, clientWidth - 2 * margin);
    const int textBoxHeight =
        std::max(0, uiTextHeight_ + windowSettings_.textBoxMargin);
    const int settingsButtonWidth = textBoxHeight;
    const int textBoxWidth =
        std::max(0, originalTextBoxWidth - margin - settingsButtonWidth);
    const int textBoxX = margin;
    const int textBoxY = std::max(0, clientHeight - margin - textBoxHeight);
    const int settingsButtonX = textBoxX + textBoxWidth + margin;

    const int listBoxX = margin;
    const int listBoxY = margin;
    const int listBoxWidth = std::max(0, clientWidth - 2 * margin);
    const int listBoxHeight =
        std::max(0, clientHeight - 3 * margin - textBoxHeight);

    MoveWindow(filterTextBox_, textBoxX, textBoxY, textBoxWidth, textBoxHeight,
               TRUE);
    MoveWindow(settingsButton_, settingsButtonX, textBoxY, settingsButtonWidth,
               textBoxHeight, TRUE);
    MoveWindow(listBox_, listBoxX, listBoxY, listBoxWidth, listBoxHeight, TRUE);
}

void ClipListView::Destroy() {
    if (parent_) {
        KillTimer(parent_, kDebounceTimerId);
    }

    ReleaseControlResources();
    visibleItemIndices_.clear();
    items_ = nullptr;
    listBox_ = nullptr;
    filterTextBox_ = nullptr;
    settingsButton_ = nullptr;
    settingsToolTip_ = nullptr;
    uiTextHeight_ = 0;
    parent_ = nullptr;
}

void ClipListView::SetItems(const std::vector<ClipItem> &items) {
    items_ = &items;
    ApplyCurrentFilter();
}

ClipListView::Event ClipListView::HandleCommand(HWND parent, WPARAM wParam,
                                                LPARAM lParam) {
    if (parent_ != parent) {
        return Event::None;
    }

    const int controlId = LOWORD(wParam);
    const int notifyCode = HIWORD(wParam);

    if (controlId == kFilterTextBoxControlId &&
        reinterpret_cast<HWND>(lParam) == filterTextBox_ &&
        notifyCode == EN_CHANGE) {
        StartFilterDebounce(parent);
        return Event::None;
    }

    if (controlId == kListBoxControlId &&
        reinterpret_cast<HWND>(lParam) == listBox_ &&
        notifyCode == LBN_DBLCLK) {
        return Event::ActivateSelectedItem;
    }

    if (controlId == kSettingsButtonControlId &&
        reinterpret_cast<HWND>(lParam) == settingsButton_ &&
        notifyCode == BN_CLICKED) {
        return Event::OpenSettings;
    }

    return Event::None;
}

bool ClipListView::HandleTimer(HWND parent, WPARAM timerId) {
    if (parent_ != parent || timerId != kDebounceTimerId) {
        return false;
    }

    KillTimer(parent_, kDebounceTimerId);
    ApplyCurrentFilter();
    return true;
}

void ClipListView::FocusFilterBox() const {
    if (filterTextBox_ && IsWindow(filterTextBox_)) {
        SetFocus(filterTextBox_);
    }
}

bool ClipListView::SelectFirstVisibleItem() const {
    if (!listBox_ || !IsWindow(listBox_)) {
        return false;
    }

    const int itemCount =
        static_cast<int>(SendMessageW(listBox_, LB_GETCOUNT, 0, 0));
    if (itemCount <= 0 || itemCount == LB_ERR) {
        return false;
    }

    SendMessageW(listBox_, LB_SETCURSEL, 0, 0);
    return true;
}

std::optional<size_t> ClipListView::GetSelectedItemIndex() const {
    if (!listBox_) {
        return std::nullopt;
    }

    const int selectedRow =
        static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
    if (selectedRow == LB_ERR || selectedRow < 0 ||
        selectedRow >= static_cast<int>(visibleItemIndices_.size())) {
        return std::nullopt;
    }

    return visibleItemIndices_[selectedRow];
}

LRESULT CALLBACK ClipListView::ListBoxProcThunk(HWND hwnd, UINT message,
                                                WPARAM wParam, LPARAM lParam) {
    auto *self = reinterpret_cast<ClipListView *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self || !self->originalListBoxProc_) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == WM_KEYDOWN && wParam == VK_RETURN && self->parent_) {
        SendMessageW(self->parent_, WM_COMMAND,
                     MAKEWPARAM(kListBoxControlId, LBN_DBLCLK),
                     reinterpret_cast<LPARAM>(hwnd));
        return 0;
    }

    if (message == WM_CHAR &&
        self->RedirectPrintableCharToFilter(wParam, lParam)) {
        return 0;
    }

    return CallWindowProcW(self->originalListBoxProc_, hwnd, message, wParam,
                           lParam);
}

LRESULT CALLBACK ClipListView::FilterTextBoxProcThunk(HWND hwnd, UINT message,
                                                      WPARAM wParam,
                                                      LPARAM lParam) {
    auto *self = reinterpret_cast<ClipListView *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self || !self->originalFilterTextBoxProc_) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == WM_KEYDOWN) {
        if (wParam == VK_DOWN) {
            self->MoveSelection(1);
            return 0;
        }

        if (wParam == VK_UP) {
            self->MoveSelection(-1);
            return 0;
        }
    }

    return CallWindowProcW(self->originalFilterTextBoxProc_, hwnd, message,
                           wParam, lParam);
}

HFONT ClipListView::CreateSystemUiFont() const {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                               &metrics, 0)) {
        return nullptr;
    }

    return CreateFontIndirectW(&metrics.lfMessageFont);
}

void ClipListView::ReleaseControlResources() {
    if (listBox_ && IsWindow(listBox_) && originalListBoxProc_) {
        SetWindowLongPtrW(listBox_, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(originalListBoxProc_));
    }
    originalListBoxProc_ = nullptr;

    if (filterTextBox_ && IsWindow(filterTextBox_) &&
        originalFilterTextBoxProc_) {
        SetWindowLongPtrW(
            filterTextBox_, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(originalFilterTextBoxProc_));
    }
    originalFilterTextBoxProc_ = nullptr;

    if (settingsToolTip_ && IsWindow(settingsToolTip_)) {
        DestroyWindow(settingsToolTip_);
        settingsToolTip_ = nullptr;
    }

    if (settingsIcon_) {
        DestroyIcon(settingsIcon_);
        settingsIcon_ = nullptr;
    }

    if (uiFont_) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
    }
}

bool ClipListView::RedirectPrintableCharToFilter(WPARAM wParam,
                                                 LPARAM lParam) const {
    if (!filterTextBox_ || !IsWindow(filterTextBox_)) {
        return false;
    }

    const wchar_t typedChar = static_cast<wchar_t>(wParam);
    if (typedChar < L' ' || typedChar == 0x7F) {
        return false;
    }

    const UINT repeatCount = std::max<UINT>(1, LOWORD(lParam));
    std::wstring insertedText(repeatCount, typedChar);
    SetFocus(filterTextBox_);
    SendMessageW(filterTextBox_, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(insertedText.c_str()));
    return true;
}

bool ClipListView::MoveSelection(int delta) const {
    if (!listBox_ || !IsWindow(listBox_)) {
        return false;
    }

    const int itemCount =
        static_cast<int>(SendMessageW(listBox_, LB_GETCOUNT, 0, 0));
    if (itemCount <= 0 || itemCount == LB_ERR) {
        return false;
    }

    int selectedRow =
        static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
    if (selectedRow == LB_ERR) {
        selectedRow = delta >= 0 ? 0 : itemCount - 1;
    } else {
        selectedRow = std::clamp(selectedRow + delta, 0, itemCount - 1);
    }

    SendMessageW(listBox_, LB_SETCURSEL, selectedRow, 0);
    return true;
}

void ClipListView::ApplyCurrentFilter() {
    if (!listBox_) {
        return;
    }

    const std::optional<size_t> previousSelectedItemIndex =
        GetSelectedItemIndex();
    const std::wstring filter = ReadFilterText();

    SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
    visibleItemIndices_.clear();

    if (!items_) {
        return;
    }

    for (size_t index = 0; index < items_->size(); ++index) {
        const ClipItem &item = (*items_)[index];
        if (!filter.empty() && item.key.find(filter) == std::wstring::npos) {
            continue;
        }

        visibleItemIndices_.push_back(index);
        const std::wstring displayText = item.GetDisplayText();
        SendMessageW(listBox_, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(displayText.c_str()));
    }

    if (previousSelectedItemIndex) {
        const auto selectedItem =
            std::find(visibleItemIndices_.begin(), visibleItemIndices_.end(),
                      *previousSelectedItemIndex);
        if (selectedItem != visibleItemIndices_.end()) {
            const int selectedRow =
                static_cast<int>(selectedItem - visibleItemIndices_.begin());
            SendMessageW(listBox_, LB_SETCURSEL, selectedRow, 0);
            return;
        }
    }

    SelectFirstVisibleItem();
}

void ClipListView::StartFilterDebounce(HWND parent) const {
    SetTimer(parent, kDebounceTimerId, kDebounceDelayMs, nullptr);
}

std::wstring ClipListView::ReadFilterText() const {
    if (!filterTextBox_) {
        return L"";
    }

    const int textLength = GetWindowTextLengthW(filterTextBox_);
    if (textLength <= 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(textLength) + 1, L'\0');
    GetWindowTextW(filterTextBox_, buffer.data(), textLength + 1);
    return std::wstring(buffer.data());
}

} // namespace ClipDeck
