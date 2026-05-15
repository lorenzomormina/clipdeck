cmake_minimum_required(VERSION 3.10.0)
project(ClipDeck VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    ClipDeck
    WIN32
    winres/resource.rc
    src/main.cpp
    src/AppConfig.cpp
    src/ClipDeckApp.cpp
    src/ClipboardUtils.cpp
    src/ClipListView.cpp
    src/GlobalHotkey.cpp
    src/HotkeyParser.cpp
    src/MainWindow.cpp
    src/SettingsWindow.cpp
    src/TrayIcon.cpp
)

target_compile_definitions(
    ClipDeck
    PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
)

target_link_libraries(
    ClipDeck
    PRIVATE
        comctl32
)

target_include_directories(
    ClipDeck
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/winres
)

add_custom_command(TARGET ClipDeck POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/settings.txt
            $<TARGET_FILE_DIR:ClipDeck>/settings.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/files
            $<TARGET_FILE_DIR:ClipDeck>/files
)
