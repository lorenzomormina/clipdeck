#include "ClipDeckApp.h"

#include <utility>
#include <windows.h>

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR,
                    _In_ int nCmdShow) {
    ClipDeck::AppConfig config = ClipDeck::LoadAppConfig();
    ClipDeck::ClipDeckApp app(instance, std::move(config));
    return app.Run(nCmdShow);
}
