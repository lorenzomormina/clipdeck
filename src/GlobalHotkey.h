#pragma once

#include <windows.h>

namespace ClipDeck {

class GlobalHotkey {
  public:
    ~GlobalHotkey();

    bool Register(HWND owner, int id, UINT modifiers, UINT virtualKey);
    void Unregister();
    bool Matches(WPARAM wParam) const;

  private:
    HWND owner_ = nullptr;
    int id_ = 0;
};

} // namespace ClipDeck
