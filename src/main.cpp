#include <QApplication>
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EasyShotter");
    app.setApplicationVersion("0.1.0");
    app.setQuitOnLastWindowClosed(false);

    easyshotter::MainWindow mainWindow;
    mainWindow.hide();

    return app.exec();
}
