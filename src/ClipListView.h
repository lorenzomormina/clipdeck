#pragma once

#include "AppConfig.h"

#include <optional>
#include <vector>

#include <windows.h>

namespace clipass {

class ClipListView {
  public:
    enum class Event { None, ActivateSelectedItem, OpenSettings };

    ~ClipListView();

    bool Create(HWND parent, HINSTANCE instance, WindowSettings settings);
    void Destroy();

    void SetItems(const std::vector<ClipItem> &items);
    Event HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam);
    bool HandleTimer(HWND parent, WPARAM timerId);
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

    HFONT CreateSystemUiFont() const;
    void ReleaseControlResources();
    void LayoutToParentClientArea();
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

    const std::vector<ClipItem> *items_ = nullptr;
    std::vector<size_t> visibleItemIndices_;
    WindowSettings windowSettings_;
    int uiTextHeight_ = 0;
};

} // namespace clipass
