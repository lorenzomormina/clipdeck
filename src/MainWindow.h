#pragma once

#include "AppConfig.h"
#include "ClipboardUtils.h"

#include <windows.h>

namespace clipass {

class MainWindow {
  public:
    MainWindow(HINSTANCE instance, AppConfig config);
    ~MainWindow();

    int Run(int nCmdShow);

  private:
    static constexpr wchar_t kWindowClassName[] = L"ClipassMainWindow";
    static constexpr wchar_t kWindowTitle[] = L"clipass";
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr int kTrayOpenCommandId = 1001;
    static constexpr int kTrayExitCommandId = 1002;
    static constexpr int kHotkeyId = 0xBEEF;

    bool CreateMainWindow();
    bool RegisterWindowClass() const;

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT OnCreate();
    void OnDestroy();
    void OnHotkey();
    void OnTrayIconMessage(LPARAM lParam);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void ToggleVisibility();
    void ShowWindowAndActivate();
    void HideWindow();
    void PopulateListBox() const;
    void ActivateSelectedItem();
    bool RegisterGlobalHotkey() const;
    bool AddTrayIcon();
    void RemoveTrayIcon();
    void ReleaseUiResources();
    HFONT CreateSystemUiFont() const;

    static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ListBoxProcThunk(HWND hwnd, UINT message,
                                             WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_;
    AppConfig config_;
    HWND hwnd_ = nullptr;
    HWND listBox_ = nullptr;
    HFONT uiFont_ = nullptr;
    HICON trayIcon_ = nullptr;
    WNDPROC originalListBoxProc_ = nullptr;
    FocusSnapshot lastFocus_;
};

} // namespace clipass
