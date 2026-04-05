cmake_minimum_required(VERSION 3.10.0)
project(clipass VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    clipass
    WIN32
    src/main.cpp
    src/AppConfig.cpp
    src/ClipboardUtils.cpp
    src/MainWindow.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
