#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include "remindme/app_info.hpp"
#include "remindme/main_window.hpp"
#include "remindme/theme.hpp"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
constexpr int kActivateTimeoutMs = 2000;

// One name per user, so two Windows accounts don't talk to each other's copy.
QString instanceServerName()
{
    const QByteArray user = qEnvironmentVariable("USERNAME").toUtf8();
    return QString("RemindMe-%1").arg(QString::fromLatin1(QCryptographicHash::hash(user, QCryptographicHash::Sha1).toHex().left(12)));
}

// Asks a running copy to show its window. True when it answered.
bool activateRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(instanceServerName());
    if (!socket.waitForConnected(kActivateTimeoutMs))
        return false;

    socket.write("show\n");
    if (!socket.waitForBytesWritten(kActivateTimeoutMs))
        return false;
    return socket.waitForReadyRead(kActivateTimeoutMs) && socket.readAll().startsWith("ok");
}

// Ends a stuck copy, but only if the lock's PID really is a RemindMe executable (PIDs get reused).
bool terminateStuckInstance(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return false;

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    bool ended = false;
    if (QueryFullProcessImageNameW(process, 0, path, &size))
    {
        const QString exe = QFileInfo(QString::fromWCharArray(path, static_cast<int>(size))).fileName().toLower();
        const QString self = QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower();
        if (exe == self && TerminateProcess(process, 1))
            ended = WaitForSingleObject(process, 5000) == WAIT_OBJECT_0;
    }
    CloseHandle(process);
    return ended;
#else
    Q_UNUSED(pid);
    return false;
#endif
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(remindme::AppInfo::kAppName);
    app.setApplicationVersion(remindme::AppInfo::kAppVersion);
    app.setWindowIcon(QIcon(":/icons/app.png"));
    // RemindMe lives in the tray: closing a reminder popup or the main window must never end it.
    // Quitting is explicit (tray Quit, or closing the window when there is no tray).
    app.setQuitOnLastWindowClosed(false);
    remindme::Theme::apply(remindme::Theme::systemPrefersDark());

    QString lockRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (lockRoot.isEmpty())
        lockRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockRoot.isEmpty())
        QDir().mkpath(lockRoot);

    QLockFile singleInstanceLock(QDir(lockRoot).filePath("remindme_single_instance.lock"));
    singleInstanceLock.setStaleLockTime(0);
    if (!singleInstanceLock.tryLock(0))
    {
        if (activateRunningInstance())
            return 0;

        qint64 pid = 0;
        QString host;
        QString appName;
        singleInstanceLock.getLockInfo(&pid, &host, &appName);
        const QMessageBox::StandardButton answer = QMessageBox::question(
            nullptr,
            "RemindMe isn't responding",
            "Another copy of RemindMe is running but isn't responding, so its window can't be shown.\n\n"
            "End that copy and start RemindMe again? Your reminders are saved.",
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return 0;

        if (!terminateStuckInstance(pid))
        {
            QMessageBox::warning(
                nullptr,
                "Couldn't end RemindMe",
                "The other copy couldn't be ended. Close RemindMe.exe in Task Manager, then start RemindMe again.");
            return 0;
        }
        singleInstanceLock.removeStaleLockFile();
        if (!singleInstanceLock.tryLock(2000))
        {
            QMessageBox::warning(nullptr, "Couldn't start RemindMe", "RemindMe is still locked by another copy. Try again in a moment.");
            return 0;
        }
    }

    remindme::MainWindow w;
    w.setWindowIcon(QIcon(":/icons/app.png"));
    w.show();

    // Later launches connect here and ask this copy to show itself instead of starting another.
    QLocalServer instanceServer;
    QLocalServer::removeServer(instanceServerName());
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &w, [&]()
                     {
                         while (QLocalSocket *client = instanceServer.nextPendingConnection())
                         {
                             QObject::connect(client, &QLocalSocket::readyRead, &w, [&w, client]()
                                              {
                                                  if (client->readAll().startsWith("show"))
                                                  {
                                                      w.showFromTray();
                                                      client->write("ok\n");
                                                      client->flush();
                                                  }
                                                  client->disconnectFromServer(); });
                             QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
                         }
                     });
    instanceServer.listen(instanceServerName());

    const int code = app.exec();
    // Release the lock before teardown, so a slow or stuck shutdown can't lock out the next launch.
    instanceServer.close();
    singleInstanceLock.unlock();
    return code;
}
