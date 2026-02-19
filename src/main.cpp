#include <QApplication>
#include <QIcon>
#include "remindme/app_info.hpp"
#include "remindme/main_window.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(remindme::AppInfo::kAppName);
    app.setApplicationVersion(remindme::AppInfo::kAppVersion);
    app.setWindowIcon(QIcon(":/icons/app.png"));

    remindme::MainWindow w;
    w.setWindowIcon(QIcon(":/icons/app.png"));
    w.show();

    return app.exec();
}
