cmake_minimum_required(VERSION 3.21)
project(ffmpeg-qt6 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ffmpeg-qt6
    src/main.cpp
    src/MainWindow.cpp
    src/FfmpegProcess.cpp
    src/EncodeSettings.cpp
)

target_link_libraries(ffmpeg-qt6 PRIVATE Qt6::Widgets)
