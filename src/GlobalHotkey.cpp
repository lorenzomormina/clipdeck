#include "GlobalHotkey.h"

namespace clipass {

GlobalHotkey::~GlobalHotkey() { Unregister(); }

bool GlobalHotkey::Register(HWND owner, int id, UINT modifiers,
                            UINT virtualKey) {
    Unregister();

    if (!RegisterHotKey(owner, id, modifiers, virtualKey)) {
        return false;
    }

    owner_ = owner;
    id_ = id;
    return true;
}

void GlobalHotkey::Unregister() {
    if (!owner_) {
        return;
    }

    UnregisterHotKey(owner_, id_);
    owner_ = nullptr;
    id_ = 0;
}

bool GlobalHotkey::Matches(WPARAM wParam) const {
    return owner_ != nullptr && wParam == static_cast<WPARAM>(id_);
}

} // namespace clipass
