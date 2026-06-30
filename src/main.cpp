#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ffmpeg-qt6");
    MainWindow w;
    w.show();
    return app.exec();
}
