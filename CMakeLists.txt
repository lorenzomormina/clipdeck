cmake_minimum_required(VERSION 3.10.0)
project(clipass VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    clipass
    WIN32
    resources/resource.rc
    src/main.cpp
    src/AppConfig.cpp
    src/ClipboardUtils.cpp
    src/ClipListView.cpp
    src/GlobalHotkey.cpp
    src/HotkeyParser.cpp
    src/MainWindow.cpp
    src/SettingsWindow.cpp
    src/TrayIcon.cpp
)

target_compile_definitions(
    clipass
    PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
)

target_link_libraries(
    clipass
    PRIVATE
        comctl32
)

add_custom_command(TARGET clipass POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons
            $<TARGET_FILE_DIR:clipass>/icons
)

add_custom_command(TARGET clipass POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/config.txt
            $<TARGET_FILE_DIR:clipass>/config.txt
)
