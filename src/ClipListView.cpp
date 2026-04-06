#define UNICODE

#include "ClipListView.h"

#include <string>

namespace clipass {

ClipListView::~ClipListView() { Destroy(); }

bool ClipListView::Create(HWND parent, HINSTANCE instance,
                          WindowSettings windowSettings) {
    parent_ = parent;
    windowSettings_ = windowSettings;

    int margin = windowSettings.margin;

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
        int textHeight = tm.tmHeight;

        SelectObject(hdc, oldFont);
        ReleaseDC(parent_, hdc);
        fontHeight = textHeight;
    }

    RECT clientRect;
    GetClientRect(parent_, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    int textBoxWidth = clientWidth - 2 * margin;
    int textBoxHeight = fontHeight + windowSettings.textBoxMargin;
    int textBoxX = margin;
    int textBoxY = clientHeight - margin - textBoxHeight;

    int listBoxX = margin;
    int listBoxY = margin;
    int listBoxWidth = clientWidth - 2 * margin;
    int listBoxHeight = clientHeight - 3 * margin - textBoxHeight;

    MoveWindow(filterTextBox_, textBoxX, textBoxY, textBoxWidth, textBoxHeight,
               FALSE);

    MoveWindow(listBox_, listBoxX, listBoxY, listBoxWidth, listBoxHeight,
               FALSE);

    SetWindowLongPtrW(listBox_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    originalListBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        listBox_, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&ClipListView::ListBoxProcThunk)));

    return true;
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

    return CallWindowProcW(self->originalListBoxProc_, hwnd, message, wParam,
                           lParam);
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

    if (uiFont_) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
    }
}

void ClipListView::ApplyCurrentFilter() {
    if (!listBox_) {
        return;
    }

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

} // namespace clipass
