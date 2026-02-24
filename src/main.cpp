#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include "remindme/app_info.hpp"
#include "remindme/main_window.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(remindme::AppInfo::kAppName);
    app.setApplicationVersion(remindme::AppInfo::kAppVersion);
    app.setWindowIcon(QIcon(":/icons/app.png"));

    QString lockRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (lockRoot.isEmpty())
        lockRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockRoot.isEmpty())
        QDir().mkpath(lockRoot);

    QLockFile singleInstanceLock(QDir(lockRoot).filePath("remindme_single_instance.lock"));
    singleInstanceLock.setStaleLockTime(0);
    if (!singleInstanceLock.tryLock(0))
    {
        QMessageBox::information(
            nullptr,
            "Already Running",
            "RemindMe is already running. If you do not see it, check the system tray.");
        return 0;
    }

    remindme::MainWindow w;
    w.setWindowIcon(QIcon(":/icons/app.png"));
    w.show();

    return app.exec();
}
