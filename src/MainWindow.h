#pragma once

#include "AppConfig.h"
#include "ClipListView.h"
#include "ClipboardUtils.h"
#include "GlobalHotkey.h"
#include "TrayIcon.h"

#include <functional>

#include <windows.h>

namespace ClipDeck {

class MainWindow {
  public:
    MainWindow(HINSTANCE instance, const AppConfig &config);

    bool Create(int nCmdShow);
    bool PreTranslateMessage(const MSG &message);
    void ApplyConfig();
    void SetOpenSettingsCallback(std::function<bool()> callback);
    void SetReloadConfigCallback(std::function<void()> callback);
    void SetSettingsActiveCallback(std::function<bool()> callback);

  private:
    static constexpr wchar_t kWindowClassName[] = L"ClipDeckMainWindow";
    static constexpr wchar_t kWindowTitle[] = L"ClipDeck";
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr int kToggleHotkeyId = 1006;

    bool RegisterWindowClass() const;
    bool CreateMainWindow();
    bool RegisterGlobalHotkey();
    void OpenSettingsWindow();

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);

    LRESULT OnCreate();
    void OnDestroy();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnTrayCallback(LPARAM lParam);
    void OnTrayMenuAction(TrayIcon::MenuAction action);
    void OnClipListEvent(ClipListView::Event event);
    void OnHotkey();
    void ActivateSelectedItem(bool copyOnly);

    void ToggleVisibility();
    void ShowWindowAndActivate();
    void HideWindow();

    HINSTANCE instance_;
    const AppConfig &config_;
    HWND hwnd_ = nullptr;
    TrayIcon trayIcon_;
    GlobalHotkey hotkey_;
    ClipListView clipListView_;
    FocusSnapshot lastFocus_;
    std::function<bool()> openSettingsCallback_;
    std::function<void()> reloadConfigCallback_;
    std::function<bool()> isSettingsActiveCallback_;
};

} // namespace ClipDeck
