#include "ClipListView.h"
#include "resource.h"
#include "utils.h"

#include <algorithm>
#include <commctrl.h>
#include <sstream>
#include <string>
#include <vector>

namespace ClipDeck {

namespace {
wchar_t kConfigurationToolTipText[] = L"Configuration";
wchar_t kGroupToggleToolTipText[] = L"Show or hide groups";

bool IsControlKeyDown() { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
} // namespace

ClipListView::~ClipListView() { Destroy(); }

bool ClipListView::Create(HWND parent, HINSTANCE instance,
                          MainWindowSettings mainWindowSettings) {
    parent_ = parent;
    mainWindowSettings_ = mainWindowSettings;
    uiTextHeight_ = 0;
    groupListVisible_ = false;

    groupToggleButton_ = CreateWindowExW(
        0, L"BUTTON", nullptr, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON,
        0, 0, 0, 0, parent_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kGroupToggleButtonControlId)),
        instance, nullptr);
    if (!groupToggleButton_) {
        Destroy();
        return false;
    }

    selectedGroupTextBox_ = CreateWindowExW(
        0, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 0, 0,
        0, 0, parent_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSelectedGroupTextBoxControlId)),
        instance, nullptr);
    if (!selectedGroupTextBox_) {
        Destroy();
        return false;
    }

    groupListBox_ = CreateWindowExW(
        0, L"LISTBOX", nullptr,
        WS_CHILD | LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, parent_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGroupListBoxControlId)),
        instance, nullptr);
    if (!groupListBox_) {
        Destroy();
        return false;
    }

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

    groupToggleToolTip_ =
        CreateToolTip(instance, groupToggleButton_, kGroupToggleToolTipText);
    settingsToolTip_ =
        CreateToolTip(instance, settingsButton_, kConfigurationToolTipText);

    uiFont_ = CreateSystemUiFont();
    if (uiFont_) {
        SendMessageW(groupListBox_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(uiFont_), TRUE);
        SendMessageW(selectedGroupTextBox_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(uiFont_), TRUE);
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
    groupListOpenIcon_ = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_GROUP_LIST_OPEN), IMAGE_ICON, iconSize,
        iconSize, LR_DEFAULTCOLOR));
    groupListCloseIcon_ = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_GROUP_LIST_CLOSE), IMAGE_ICON, iconSize,
        iconSize, LR_DEFAULTCOLOR));
    settingsIcon_ = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(IDI_SETTINGS), IMAGE_ICON,
                   iconSize, iconSize, LR_DEFAULTCOLOR));
    if (!groupListOpenIcon_ || !groupListCloseIcon_ || !settingsIcon_) {
        Destroy();
        return false;
    }

    UpdateGroupToggleIcon();
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

void ClipListView::SetMainWindowSettings(MainWindowSettings settings) {
    mainWindowSettings_ = settings;
}

void ClipListView::Layout(int clientWidth, int clientHeight) {
    if (!parent_ || !groupToggleButton_ || !selectedGroupTextBox_ ||
        !groupListBox_ || !listBox_ || !filterTextBox_ || !settingsButton_) {
        return;
    }

    const int margin = std::max(0, mainWindowSettings_.margin);
    const int controlHeight =
        std::max(0, uiTextHeight_ + mainWindowSettings_.textBoxMargin);
    const int availableWidth = std::max(0, clientWidth - 2 * margin);

    const int topRowY = margin;
    const int groupButtonWidth = controlHeight;
    const int selectedGroupTextBoxX = margin + groupButtonWidth + margin;
    const int selectedGroupTextBoxWidth =
        std::max(0, clientWidth - selectedGroupTextBoxX - margin);

    const int bottomRowY =
        std::max(0, clientHeight - margin - controlHeight);
    const int settingsButtonWidth = controlHeight;
    const int filterTextBoxWidth =
        std::max(0, availableWidth - margin - settingsButtonWidth);
    const int filterTextBoxX = margin;
    const int settingsButtonX = filterTextBoxX + filterTextBoxWidth + margin;

    const int contentY = topRowY + controlHeight + margin;
    const int contentHeight =
        std::max(0, bottomRowY - margin - contentY);
    const int groupListBoxX = margin;
    const int groupListBoxWidth =
        std::max(0, mainWindowSettings_.groupListBoxWidth);
    const int listBoxX = groupListVisible_
                             ? groupListBoxX + groupListBoxWidth + margin
                             : margin;
    const int listBoxWidth = groupListVisible_
                                 ? std::max(0, clientWidth - listBoxX - margin)
                                 : availableWidth;

    MoveWindow(groupToggleButton_, margin, topRowY, groupButtonWidth,
               controlHeight, TRUE);
    MoveWindow(selectedGroupTextBox_, selectedGroupTextBoxX, topRowY,
               selectedGroupTextBoxWidth, controlHeight, TRUE);
    MoveWindow(groupListBox_, groupListBoxX, contentY, groupListBoxWidth,
               contentHeight, TRUE);
    MoveWindow(listBox_, listBoxX, contentY, listBoxWidth, contentHeight, TRUE);
    MoveWindow(filterTextBox_, filterTextBoxX, bottomRowY,
               filterTextBoxWidth, controlHeight, TRUE);
    MoveWindow(settingsButton_, settingsButtonX, bottomRowY,
               settingsButtonWidth, controlHeight, TRUE);
    ShowWindow(groupListBox_, groupListVisible_ ? SW_SHOW : SW_HIDE);
}

void ClipListView::Destroy() {
    if (parent_) {
        KillTimer(parent_, kDebounceTimerId);
    }

    ReleaseControlResources();
    allItemRefs_.clear();
    visibleItemRefs_.clear();
    groups_ = nullptr;
    selectedGroupKey_.clear();
    groupToggleButton_ = nullptr;
    selectedGroupTextBox_ = nullptr;
    groupListBox_ = nullptr;
    listBox_ = nullptr;
    filterTextBox_ = nullptr;
    settingsButton_ = nullptr;
    groupToggleToolTip_ = nullptr;
    settingsToolTip_ = nullptr;
    uiTextHeight_ = 0;
    groupListVisible_ = false;
    parent_ = nullptr;
}

void ClipListView::SetGroups(const std::vector<Group> &groups) {
    const std::wstring previousGroupKey = selectedGroupKey_;
    groups_ = &groups;
    allItemRefs_.clear();

    for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const Group &group = groups[groupIndex];
        for (size_t itemIndex = 0; itemIndex < group.items.size();
             ++itemIndex) {
            allItemRefs_.push_back({groupIndex, itemIndex});
        }
    }

    std::sort(allItemRefs_.begin(), allItemRefs_.end(),
              [this](ItemRef left, ItemRef right) {
                  const ClipItem *leftItem = ResolveItemRef(left);
                  const ClipItem *rightItem = ResolveItemRef(right);
                  if (!leftItem || !rightItem) {
                      return false;
                  }

                  return leftItem->loadOrder < rightItem->loadOrder;
              });

    SelectGroupByKeyOrFallback(previousGroupKey);
    PopulateGroupList();
    UpdateSelectedGroupText();
    ApplyCurrentFilter();
}

ClipListView::Event ClipListView::HandleCommand(HWND parent, WPARAM wParam,
                                                LPARAM lParam) {
    if (parent_ != parent) {
        return Event::None;
    }

    const int controlId = LOWORD(wParam);
    const int notifyCode = HIWORD(wParam);

    if (controlId == kGroupToggleButtonControlId &&
        reinterpret_cast<HWND>(lParam) == groupToggleButton_ &&
        notifyCode == BN_CLICKED) {
        groupListVisible_ = !groupListVisible_;
        UpdateGroupToggleIcon();
        RECT clientRect = {};
        GetClientRect(parent_, &clientRect);
        Layout(clientRect.right - clientRect.left,
               clientRect.bottom - clientRect.top);
        return Event::None;
    }

    if (controlId == kGroupListBoxControlId &&
        reinterpret_cast<HWND>(lParam) == groupListBox_ &&
        notifyCode == LBN_SELCHANGE) {
        const int selectedRow =
            static_cast<int>(SendMessageW(groupListBox_, LB_GETCURSEL, 0, 0));
        SelectGroupAtRow(selectedRow);
        return Event::None;
    }

    if (controlId == kFilterTextBoxControlId &&
        reinterpret_cast<HWND>(lParam) == filterTextBox_ &&
        notifyCode == EN_CHANGE) {
        StartFilterDebounce(parent);
        return Event::None;
    }

    if (controlId == kListBoxControlId &&
        reinterpret_cast<HWND>(lParam) == listBox_ &&
        notifyCode == LBN_DBLCLK) {
        return IsControlKeyDown() ? Event::ActivateSelectedItemCopyOnly
                                  : Event::ActivateSelectedItem;
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

const ClipItem *ClipListView::GetSelectedItem() const {
    const std::optional<ItemRef> selectedItemRef = GetSelectedItemRef();
    if (!selectedItemRef) {
        return nullptr;
    }

    return ResolveItemRef(*selectedItemRef);
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

HWND ClipListView::CreateToolTip(HINSTANCE instance, HWND tool,
                                 wchar_t *text) const {
    HWND toolTip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, parent_, nullptr, instance, nullptr);
    if (!toolTip) {
        return nullptr;
    }

    TOOLINFOW toolInfo = {};
#ifdef TTTOOLINFOW_V2_SIZE
    toolInfo.cbSize = TTTOOLINFOW_V2_SIZE;
#else
    toolInfo.cbSize = sizeof(toolInfo);
#endif
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.hwnd = parent_;
    toolInfo.uId = reinterpret_cast<UINT_PTR>(tool);
    toolInfo.lpszText = text;

    if (SendMessageW(toolTip, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&toolInfo))) {
        return toolTip;
    }

#ifdef TTTOOLINFOW_V1_SIZE
    toolInfo.cbSize = TTTOOLINFOW_V1_SIZE;
    if (SendMessageW(toolTip, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&toolInfo))) {
        return toolTip;
    }
#endif

    DestroyWindow(toolTip);
    return nullptr;
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

    if (groupToggleToolTip_ && IsWindow(groupToggleToolTip_)) {
        DestroyWindow(groupToggleToolTip_);
        groupToggleToolTip_ = nullptr;
    }

    if (settingsToolTip_ && IsWindow(settingsToolTip_)) {
        DestroyWindow(settingsToolTip_);
        settingsToolTip_ = nullptr;
    }

    if (groupListOpenIcon_) {
        DestroyIcon(groupListOpenIcon_);
        groupListOpenIcon_ = nullptr;
    }

    if (groupListCloseIcon_) {
        DestroyIcon(groupListCloseIcon_);
        groupListCloseIcon_ = nullptr;
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

void ClipListView::ApplyCurrentFilter(bool preserveSelection) {
    if (!listBox_) {
        return;
    }

    const std::optional<ItemRef> previousSelectedItemRef =
        preserveSelection ? GetSelectedItemRef() : std::nullopt;
    const std::wstring filter = ReadFilterText();

    SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
    visibleItemRefs_.clear();

    const std::optional<size_t> selectedGroupIndex = GetSelectedGroupIndex();
    if (!groups_ || !selectedGroupIndex) {
        return;
    }

    for (ItemRef itemRef : allItemRefs_) {
        if (itemRef.groupIndex != *selectedGroupIndex) {
            continue;
        }

        const ClipItem *item = ResolveItemRef(itemRef);
        if (!item) {
            continue;
        }

        if (!filter.empty()) {
            std::wstring lowerFilter;
            if (!item->caseSensitiveSearchKeys ||
                !item->caseSensitiveSearchValues) {
                lowerFilter = toLower(filter);
            }

            const std::wstring keyFilterToUse =
                item->caseSensitiveSearchKeys ? filter : lowerFilter;
            const std::wstring valueFilterToUse =
                item->caseSensitiveSearchValues ? filter : lowerFilter;
            const std::wstring keyToUse =
                item->caseSensitiveSearchKeys ? item->key : toLower(item->key);
            const std::wstring valueToUse =
                item->caseSensitiveSearchValues ? item->value
                                                : toLower(item->value);

            auto splitBySpaces = [](const std::wstring &text) {
                std::vector<std::wstring> parts;
                std::wistringstream stream(text);

                std::wstring part;
                while (stream >> part) {
                    parts.push_back(part);
                }

                return parts;
            };

            auto containsFilter = [&splitBySpaces](const std::wstring &text,
                                                   const std::wstring &filter,
                                                   bool advancedSearch) {
                if (!advancedSearch) {
                    return text.find(filter) != std::wstring::npos;
                }

                const std::vector<std::wstring> parts = splitBySpaces(filter);

                for (const std::wstring &part : parts) {
                    if (text.find(part) == std::wstring::npos) {
                        return false;
                    }
                }

                return true;
            };

            const bool matchesKey =
                containsFilter(keyToUse, keyFilterToUse,
                               item->advancedSearchKeys);

            const bool matchesValue =
                item->searchValues &&
                containsFilter(valueToUse, valueFilterToUse,
                               item->advancedSearchValues);

            if (!matchesKey && !matchesValue) {
                continue;
            }
        }

        visibleItemRefs_.push_back(itemRef);
        const std::wstring displayText = item->GetDisplayText();
        SendMessageW(listBox_, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(displayText.c_str()));
    }

    if (previousSelectedItemRef) {
        const auto selectedItem =
            std::find(visibleItemRefs_.begin(), visibleItemRefs_.end(),
                      *previousSelectedItemRef);
        if (selectedItem != visibleItemRefs_.end()) {
            const int selectedRow =
                static_cast<int>(selectedItem - visibleItemRefs_.begin());
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

void ClipListView::PopulateGroupList() {
    if (!groupListBox_) {
        return;
    }

    SendMessageW(groupListBox_, LB_RESETCONTENT, 0, 0);
    if (!groups_) {
        return;
    }

    for (const Group &group : *groups_) {
        const std::wstring displayName =
            group.name.empty() ? group.key : group.name;
        SendMessageW(groupListBox_, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(displayName.c_str()));
    }

    UpdateGroupListSelection();
}

void ClipListView::SelectGroupByKeyOrFallback(
    const std::wstring &preferredGroupKey) {
    selectedGroupKey_.clear();
    if (!groups_ || groups_->empty()) {
        return;
    }

    auto findGroupKey = [this](const std::wstring &groupKey) {
        return std::find_if(groups_->begin(), groups_->end(),
                            [&groupKey](const Group &group) {
                                return group.key == groupKey;
                            });
    };

    if (!preferredGroupKey.empty()) {
        auto preferredGroup = findGroupKey(preferredGroupKey);
        if (preferredGroup != groups_->end()) {
            selectedGroupKey_ = preferredGroup->key;
            return;
        }
    }

    auto defaultGroup = findGroupKey(defaultGroupKey);
    if (defaultGroup != groups_->end()) {
        selectedGroupKey_ = defaultGroup->key;
        return;
    }

    selectedGroupKey_ = groups_->front().key;
}

void ClipListView::SelectGroupAtRow(int row) {
    if (!groups_ || row < 0 || row >= static_cast<int>(groups_->size())) {
        return;
    }

    const std::wstring newGroupKey = (*groups_)[row].key;
    if (newGroupKey == selectedGroupKey_) {
        UpdateGroupListSelection();
        return;
    }

    selectedGroupKey_ = newGroupKey;
    UpdateSelectedGroupText();
    UpdateGroupListSelection();
    ApplyCurrentFilter(false);
}

void ClipListView::UpdateSelectedGroupText() {
    if (!selectedGroupTextBox_) {
        return;
    }

    const std::wstring displayName = GetSelectedGroupDisplayName();
    SetWindowTextW(selectedGroupTextBox_, displayName.c_str());
}

void ClipListView::UpdateGroupListSelection() const {
    if (!groupListBox_) {
        return;
    }

    const std::optional<size_t> selectedGroupIndex = GetSelectedGroupIndex();
    if (!selectedGroupIndex) {
        SendMessageW(groupListBox_, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
        return;
    }

    SendMessageW(groupListBox_, LB_SETCURSEL, *selectedGroupIndex, 0);
}

void ClipListView::UpdateGroupToggleIcon() const {
    if (!groupToggleButton_) {
        return;
    }

    HICON icon = groupListVisible_ ? groupListCloseIcon_ : groupListOpenIcon_;
    SendMessageW(groupToggleButton_, BM_SETIMAGE, IMAGE_ICON,
                 reinterpret_cast<LPARAM>(icon));
}

std::optional<size_t> ClipListView::GetSelectedGroupIndex() const {
    if (!groups_) {
        return std::nullopt;
    }

    for (size_t groupIndex = 0; groupIndex < groups_->size(); ++groupIndex) {
        if ((*groups_)[groupIndex].key == selectedGroupKey_) {
            return groupIndex;
        }
    }

    return std::nullopt;
}

std::wstring ClipListView::GetSelectedGroupDisplayName() const {
    const std::optional<size_t> selectedGroupIndex = GetSelectedGroupIndex();
    if (!groups_ || !selectedGroupIndex) {
        return L"";
    }

    const Group &group = (*groups_)[*selectedGroupIndex];
    return group.name.empty() ? group.key : group.name;
}

std::optional<ClipListView::ItemRef> ClipListView::GetSelectedItemRef() const {
    if (!listBox_) {
        return std::nullopt;
    }

    const int selectedRow =
        static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
    if (selectedRow == LB_ERR || selectedRow < 0 ||
        selectedRow >= static_cast<int>(visibleItemRefs_.size())) {
        return std::nullopt;
    }

    return visibleItemRefs_[selectedRow];
}

const ClipItem *ClipListView::ResolveItemRef(ItemRef itemRef) const {
    if (!groups_ || itemRef.groupIndex >= groups_->size()) {
        return nullptr;
    }

    const Group &group = (*groups_)[itemRef.groupIndex];
    if (itemRef.itemIndex >= group.items.size()) {
        return nullptr;
    }

    return &group.items[itemRef.itemIndex];
}

} // namespace ClipDeck
