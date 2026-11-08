#include "MainWindow.hpp"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("bloody-rgb");
    app.setStyle("Fusion");
    app.setQuitOnLastWindowClosed(false);   // keep running in the tray
    MainWindow w;
    w.show();
    return app.exec();
}
