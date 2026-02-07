#include <QApplication>
#include "app/MainWindow.h"
#include "Version.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("CF Dojo");
    app.setApplicationName("CF Dojo");
    app.setApplicationVersion(CFDojo::kVersion);

    MainWindow window;
    window.show();

    return app.exec();
}
