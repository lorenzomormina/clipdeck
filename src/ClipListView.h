#pragma once

#include "AppConfig.h"

#include <optional>
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

    bool Create(HWND parent, HINSTANCE instance, MainWindowSettings settings,
                SearchSettings searchSettings);
    void Destroy();

    void SetMainWindowSettings(MainWindowSettings settings);
    void SetSearchSettings(SearchSettings settings);
    void Layout(int clientWidth, int clientHeight);
    void SetItems(const std::vector<ClipItem> &items);
    Event HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam);
    bool HandleTimer(HWND parent, WPARAM timerId);
    void FocusFilterBox() const;
    bool SelectFirstVisibleItem() const;
    std::optional<size_t> GetSelectedItemIndex() const;
    HWND GetListHandle() const { return listBox_; }
    HWND GetSearchHandle() const { return filterTextBox_; }
    HWND GetSettingsButtonHandle() const { return settingsButton_; }

  private:
    static constexpr int kListBoxControlId = 2001;
    static constexpr int kFilterTextBoxControlId = 2002;
    static constexpr int kSettingsButtonControlId = 2003;
    static constexpr UINT kDebounceTimerId = 1005;
    static constexpr UINT_PTR kDebounceDelayMs = 100;

    static LRESULT CALLBACK ListBoxProcThunk(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK FilterTextBoxProcThunk(HWND hwnd, UINT message,
                                                   WPARAM wParam,
                                                   LPARAM lParam);

    HFONT CreateSystemUiFont() const;
    void ReleaseControlResources();
    bool RedirectPrintableCharToFilter(WPARAM wParam, LPARAM lParam) const;
    bool MoveSelection(int delta) const;
    void ApplyCurrentFilter();
    void StartFilterDebounce(HWND parent) const;
    std::wstring ReadFilterText() const;

    HWND parent_ = nullptr;
    HWND listBox_ = nullptr;
    HWND filterTextBox_ = nullptr;
    HWND settingsButton_ = nullptr;
    HWND settingsToolTip_ = nullptr;
    HICON settingsIcon_ = nullptr;
    HFONT uiFont_ = nullptr;
    WNDPROC originalListBoxProc_ = nullptr;
    WNDPROC originalFilterTextBoxProc_ = nullptr;

    const std::vector<ClipItem> *items_ = nullptr;
    std::vector<size_t> visibleItemIndices_;
    MainWindowSettings mainWindowSettings_;
    SearchSettings searchSettings_;
    int uiTextHeight_ = 0;
};

} // namespace ClipDeck
