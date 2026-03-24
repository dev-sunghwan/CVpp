#include <QApplication>

#include "qt_shell_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QtShellWindow window;
    window.show();
    return app.exec();
}
