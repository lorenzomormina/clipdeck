#pragma once

#include "AppConfig.h"
#include "ClipListView.h"
#include "ClipboardUtils.h"
#include "GlobalHotkey.h"
#include "SettingsWindow.h"
#include "TrayIcon.h"

#include <windows.h>

namespace clipass {

class MainWindow {
  public:
    MainWindow(HINSTANCE instance, AppConfig config);

    int Run(int nCmdShow);

  private:
    static constexpr wchar_t kWindowClassName[] = L"ClipassMainWindow";
    static constexpr wchar_t kWindowTitle[] = L"clipass";
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr int kToggleHotkeyId = 1006;

    bool RegisterWindowClass() const;
    bool CreateMainWindow();
    bool RegisterGlobalHotkey();
    void ReloadConfig();
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
    void ActivateSelectedItem();

    void ToggleVisibility();
    void ShowWindowAndActivate();
    void HideWindow();

    HINSTANCE instance_;
    AppConfig config_;
    HWND hwnd_ = nullptr;
    TrayIcon trayIcon_;
    GlobalHotkey hotkey_;
    ClipListView clipListView_;
    SettingsWindow settingsWindow_;
    FocusSnapshot lastFocus_;
};

} // namespace clipass
