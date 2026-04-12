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
    src/MainWindow.cpp
    src/TrayIcon.cpp
)

target_compile_definitions(
    clipass
    PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
