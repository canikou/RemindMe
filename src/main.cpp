#include <QApplication>
#include <QIcon>
#include "AppInfo.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(AppInfo::kAppName);
    app.setApplicationVersion(AppInfo::kAppVersion);
    app.setWindowIcon(QIcon(":/icons/app.png"));

    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/app.png"));
    w.show();

    return app.exec();
}
