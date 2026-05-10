#pragma once

#include "AppConfig.h"

#include <optional>
#include <string>
#include <vector>

#include <windows.h>

namespace ClipDeck {

class ClipListView {
  public:
    enum class Event {
        None,
        ActivateSelectedItem,
        ActivateSelectedItemCopyOnly,
        OpenSettings
    };

    ~ClipListView();

    bool Create(HWND parent, HINSTANCE instance, MainWindowSettings settings);
    void Destroy();

    void SetMainWindowSettings(MainWindowSettings settings);
    void Layout(int clientWidth, int clientHeight);
    void SetGroups(const std::vector<Group> &groups);
    Event HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam);
    bool HandleTimer(HWND parent, WPARAM timerId);
    void FocusFilterBox() const;
    bool SelectFirstVisibleItem() const;
    const ClipItem *GetSelectedItem() const;
    HWND GetListHandle() const { return listBox_; }
    HWND GetSearchHandle() const { return filterTextBox_; }
    HWND GetSettingsButtonHandle() const { return settingsButton_; }

  private:
    static constexpr int kListBoxControlId = 2001;
    static constexpr int kFilterTextBoxControlId = 2002;
    static constexpr int kSettingsButtonControlId = 2003;
    static constexpr int kGroupToggleButtonControlId = 2004;
    static constexpr int kSelectedGroupTextBoxControlId = 2005;
    static constexpr int kGroupListBoxControlId = 2006;
    static constexpr UINT kDebounceTimerId = 1005;
    static constexpr UINT_PTR kDebounceDelayMs = 100;

    struct ItemRef {
        size_t groupIndex = 0;
        size_t itemIndex = 0;

        bool operator==(const ItemRef &other) const {
            return groupIndex == other.groupIndex &&
                   itemIndex == other.itemIndex;
        }
    };

    static LRESULT CALLBACK ListBoxProcThunk(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK FilterTextBoxProcThunk(HWND hwnd, UINT message,
                                                   WPARAM wParam,
                                                   LPARAM lParam);

    HFONT CreateSystemUiFont() const;
    void ReleaseControlResources();
    HWND CreateToolTip(HINSTANCE instance, HWND tool, wchar_t *text) const;
    bool RedirectPrintableCharToFilter(WPARAM wParam, LPARAM lParam) const;
    bool MoveSelection(int delta) const;
    void ApplyCurrentFilter(bool preserveSelection = true);
    void StartFilterDebounce(HWND parent) const;
    std::wstring ReadFilterText() const;
    void PopulateGroupList();
    void SelectGroupByKeyOrFallback(const std::wstring &preferredGroupKey);
    void SelectGroupAtRow(int row);
    void UpdateSelectedGroupText();
    void UpdateGroupListSelection() const;
    void UpdateGroupToggleIcon() const;
    std::optional<size_t> GetSelectedGroupIndex() const;
    std::wstring GetSelectedGroupDisplayName() const;
    std::optional<ItemRef> GetSelectedItemRef() const;
    const ClipItem *ResolveItemRef(ItemRef itemRef) const;

    HWND parent_ = nullptr;
    HWND groupToggleButton_ = nullptr;
    HWND selectedGroupTextBox_ = nullptr;
    HWND groupListBox_ = nullptr;
    HWND listBox_ = nullptr;
    HWND filterTextBox_ = nullptr;
    HWND settingsButton_ = nullptr;
    HWND groupToggleToolTip_ = nullptr;
    HWND settingsToolTip_ = nullptr;
    HICON groupListOpenIcon_ = nullptr;
    HICON groupListCloseIcon_ = nullptr;
    HICON settingsIcon_ = nullptr;
    HFONT uiFont_ = nullptr;
    WNDPROC originalListBoxProc_ = nullptr;
    WNDPROC originalFilterTextBoxProc_ = nullptr;

    const std::vector<Group> *groups_ = nullptr;
    std::wstring selectedGroupKey_;
    std::vector<ItemRef> allItemRefs_;
    std::vector<ItemRef> visibleItemRefs_;
    MainWindowSettings mainWindowSettings_;
    int uiTextHeight_ = 0;
    bool groupListVisible_ = false;
};

} // namespace ClipDeck
