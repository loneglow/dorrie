

#include <QApplication>
#include <QIcon>

#include "component/AppPaths.h"
#include "component/MainComponent.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("gsoldano");
    QApplication::setApplicationName("Dorrie");
    // Shown in the title bar, taskbar and alt-tab for every window.
    QApplication::setWindowIcon(QIcon(":/resources/icons/dorrie.png"));
    // Everything is stored in ~/.config/dorrie (see AppPaths.h).
    AppPaths::prepare();

    app.setStyle("Fusion");
    MainComponent mainComponent;
    mainComponent.show();
    return app.exec();
}
