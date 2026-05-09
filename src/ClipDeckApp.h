#pragma once

#include "AppConfig.h"
#include "MainWindow.h"
#include "SettingsWindow.h"

#include <windows.h>

namespace ClipDeck {

class ClipDeckApp {
  public:
    ClipDeckApp(HINSTANCE instance, AppConfig config);

    int Run(int nCmdShow);

  private:
    bool OpenSettingsWindow();
    void ReloadConfig();

    HINSTANCE instance_;
    AppConfig config_;
    SettingsWindow settingsWindow_;
    MainWindow mainWindow_;
};

} // namespace ClipDeck
