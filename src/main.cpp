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
