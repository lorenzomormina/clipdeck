#include "AppConfig.h"
#include "MainWindow.h"

#include <windows.h>

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR,
                    _In_ int nCmdShow) {
    ClipDeck::MainWindow mainWindow(instance, ClipDeck::LoadAppConfig());
    return mainWindow.Run(nCmdShow);
}
