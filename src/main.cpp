#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QLocale>
#include <QLibraryInfo>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("ffconv");
    QApplication::setApplicationName("ffconv");

    // Linux 上强制 Fusion 风格：系统 Qt 会加载 KDE Breeze 风格，其 QMenu 子菜单
    // 箭头留白不足会与 CJK 文字（如「语言」的「言」）重叠。Fusion 渲染正常，且
    // 与开发时的 ~/Qt 官方版外观一致；平台主题仍会提供系统深色调色板，故不影响配色。
    // Windows/macOS 不受影响，保留各自原生风格。
#ifdef Q_OS_LINUX
    QApplication::setStyle(QStringLiteral("Fusion"));
#endif

    // 窗口/任务栏图标：从内嵌资源按多尺寸组装，缩放时挑最接近的位图
    QIcon icon;
    for (int s : {16, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/icon-%1.png").arg(s));
    QApplication::setWindowIcon(icon);

    // 读取语言设置：默认中文（源语言），用户可在「设置→语言」改为英文
    QSettings settings;
    QString lang = settings.value("language").toString();
    if (lang.isEmpty()) {
        lang = "zh_CN";
        settings.setValue("language", lang);
    }

    // 源代码字符串为中文：zh_CN 直接用源串，无需翻译器；
    // 其它语言加载内嵌的 .qm，并加载 Qt 自带翻译让标准按钮一致。
    QTranslator appTr, qtTr;
    if (lang != "zh_CN") {
        if (appTr.load(":/i18n/ffconv_" + lang))
            app.installTranslator(&appTr);
    } else {
        if (qtTr.load("qtbase_zh_CN",
                      QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&qtTr);
    }

    MainWindow w;
    w.show();
    return app.exec();
}
