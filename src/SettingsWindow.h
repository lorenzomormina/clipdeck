#pragma once

#include "AppConfig.h"

#include <functional>
#include <string>

#include <windows.h>

namespace ClipDeck {

class SettingsWindow {
  public:
    explicit SettingsWindow(HINSTANCE instance);
    ~SettingsWindow();

    void SetConfig(const AppConfig &config);
    void SetConfigSavedCallback(std::function<void()> callback);

    bool Show();
    void Destroy();
    bool PreTranslateMessage(const MSG &message);

    HWND GetHandle() const { return hwnd_; }

  private:
    struct SettingsState {
        std::wstring originalText;
        bool isDirty = false;
        bool suppressEditChange = false;
    };

    static constexpr wchar_t kWindowClassName[] = L"ClipDeckSettingsWindow";
    static constexpr wchar_t kWindowTitle[] = L"ClipDeck settings";

    bool RegisterWindowClass() const;
    bool CreateWindowInstance();
    bool CreateControls();
    void LayoutControls() const;
    void LoadTextFromDisk();
    void Hide();
    bool TryCloseOrHide();
    bool SaveText();
    void DiscardChanges();
    bool HandleCommand(WPARAM wParam, LPARAM lParam);
    void OnEditChanged();
    std::wstring ReadEditorText() const;
    void SetEditorText(const std::wstring &text);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_;
    std::filesystem::path configPath_;
    ConfigWindowSettings configWindowSettings_;
    std::function<void()> onConfigSaved_;

    HWND hwnd_ = nullptr;
    HWND hTextArea_ = nullptr;
    HWND hSaveBtn_ = nullptr;
    HWND hCancelBtn_ = nullptr;
    HWND hSaveToolTip_ = nullptr;
    HWND hCancelToolTip_ = nullptr;
    HICON hSaveIcon_ = nullptr;
    HICON hCancelIcon_ = nullptr;
    HFONT uiFont_ = nullptr;
    SettingsState settingsState_;
};

} // namespace ClipDeck
