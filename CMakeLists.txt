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
    src/Logger.cpp
    assets/icon.qrc          # 内嵌图标资源（窗口/任务栏图标，AUTORCC 处理）
)

# Windows：把 .ico 编进 exe，让文件管理器显示程序图标
if(WIN32)
    target_sources(ffconv PRIVATE assets/app.rc)
endif()

target_link_libraries(ffconv PRIVATE Qt6::Widgets)

# 版本号单一来源：传给代码使用
target_compile_definitions(ffconv PRIVATE APP_VERSION="${PROJECT_VERSION}")

# 国际化：生成/更新 .ts（update_translations 目标），构建时 lrelease 成 .qm 并内嵌到 :/i18n
qt_add_translations(ffconv
    TS_FILES translations/ffconv_en.ts
    RESOURCE_PREFIX "/i18n"
)

# Linux 桌面集成：安装可执行文件、.desktop 与图标（应用菜单/AppImage 用）
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS ffconv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/ffconv.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon-256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME ffconv.png)
    install(FILES assets/icon.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME ffconv.svg)
endif()
