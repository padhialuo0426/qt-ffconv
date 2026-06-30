cmake_minimum_required(VERSION 3.21)
project(ffconv VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

add_executable(ffconv
    src/main.cpp
    src/MainWindow.cpp
    src/FfmpegProcess.cpp
    src/EncodeSettings.cpp
)

target_link_libraries(ffconv PRIVATE Qt6::Widgets)

# 版本号单一来源：传给代码使用
target_compile_definitions(ffconv PRIVATE APP_VERSION="${PROJECT_VERSION}")

# 国际化：生成/更新 .ts（update_translations 目标），构建时 lrelease 成 .qm 并内嵌到 :/i18n
qt_add_translations(ffconv
    TS_FILES translations/ffconv_en.ts
    RESOURCE_PREFIX "/i18n"
)
