cmake_minimum_required(VERSION 3.21)
project(ffmpeg-qt6 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

add_executable(ffmpeg-qt6
    src/main.cpp
    src/MainWindow.cpp
    src/FfmpegProcess.cpp
    src/EncodeSettings.cpp
)

target_link_libraries(ffmpeg-qt6 PRIVATE Qt6::Widgets)

# 国际化：生成/更新 .ts（update_translations 目标），构建时 lrelease 成 .qm 并内嵌到 :/i18n
qt_add_translations(ffmpeg-qt6
    TS_FILES translations/ffmpeg-qt6_en.ts
    RESOURCE_PREFIX "/i18n"
)
