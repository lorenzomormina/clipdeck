#pragma once

#include <filesystem>

#include <windows.h>

namespace clipass {

class TrayIcon {
  public:
    enum class CallbackAction { None, OpenWindow, ShowMenu };
    enum class MenuAction {
        None,
        OpenWindow,
        ReloadConfig,
        ShowConfig,
        ExitApp
    };

    ~TrayIcon();

    bool Add(HWND owner, UINT iconId, UINT callbackMessage,
             const std::filesystem::path &iconPath, const wchar_t *toolTip);
    void Remove();

    CallbackAction TranslateCallback(LPARAM lParam) const;
    MenuAction ShowContextMenu(HWND owner) const;

  private:
    static constexpr UINT kOpenCommandId = 1001;
    static constexpr UINT kExitCommandId = 1002;
    static constexpr UINT kConfigCommandId = 1003;
    static constexpr UINT kReloadCommandId = 1004;

    HICON icon_ = nullptr;
    HWND owner_ = nullptr;
    UINT iconId_ = 0;
    bool isAdded_ = false;
};

} // namespace clipass
