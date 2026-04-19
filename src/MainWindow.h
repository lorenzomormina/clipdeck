#pragma once

#include "AppConfig.h"
#include "ClipListView.h"
#include "ClipboardUtils.h"
#include "GlobalHotkey.h"
#include "TrayIcon.h"

#include <windows.h>

namespace clipass {

enum class AppView {
    MAIN_LIST,
    SETTINGS,
};

class MainWindow {
  public:
    MainWindow(HINSTANCE instance, AppConfig config);

    int Run(int nCmdShow);
    void SetView(AppView view);
    AppView GetView() const;

  private:
    struct SettingsControls {
        HWND textArea = nullptr;
        HWND saveButton = nullptr;
        HWND cancelButton = nullptr;
    };

    struct SettingsState {
        std::wstring originalText;
        bool isDirty = false;
        bool suppressEditChange = false;
    };

    static constexpr wchar_t kWindowClassName[] = L"ClipassMainWindow";
    static constexpr wchar_t kWindowTitle[] = L"clipass";
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr int kToggleHotkeyId = 1006;

    bool RegisterWindowClass() const;
    bool CreateMainWindow();
    bool RegisterGlobalHotkey();
    SettingsControls CreateSettingsControls();
    void ReloadConfig();
    void EnterSettingsView();
    void LeaveSettingsView();
    bool TryLeaveSettingsView();
    bool SaveSettingsText();
    void DiscardSettingsChanges();
    bool HandleSettingsCommand(WPARAM wParam, LPARAM lParam);
    void OnSettingsEditChanged();
    std::wstring ReadSettingsEditorText() const;
    void SetSettingsEditorText(const std::wstring &text);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);

    LRESULT OnCreate();
    void OnDestroy();
    void OnActivate(WPARAM wParam);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnTrayCallback(LPARAM lParam);
    void OnTrayMenuAction(TrayIcon::MenuAction action);
    void OnClipListEvent(ClipListView::Event event);
    void OnHotkey();
    void ActivateSelectedItem();
    void UpdateView();
    void UpdateTaskbarVisibilityForCurrentView();

    void ToggleVisibility();
    void ShowWindowAndActivate();
    void HideWindow();

    HINSTANCE instance_;
    AppConfig config_;
    HWND hwnd_ = nullptr;
    TrayIcon trayIcon_;
    GlobalHotkey hotkey_;
    ClipListView clipListView_;
    HWND hTextArea_ = nullptr;
    HWND hSaveBtn_ = nullptr;
    HWND hCancelBtn_ = nullptr;
    HFONT settingsUiFont_ = nullptr;
    SettingsState settingsState_;
    FocusSnapshot lastFocus_;
    AppView currentView_ = AppView::MAIN_LIST;
};

} // namespace clipass
