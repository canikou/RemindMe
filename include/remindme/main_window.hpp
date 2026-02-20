#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QListWidget>
#include <QHash>
#include <QStringList>
#include <QSystemTrayIcon>

#include "remindme/reminder_store.hpp"

class QAction;
namespace remindme
{

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onTick();
    void onAddClicked();
    void onExportClicked();
    void onImportClicked();

private:
    void setupSystemTray();
    void showFromTray();
    void quitFromTray();

    void refreshUI();
    void refreshCompletedPreview();
    void updateGreetingMessage();
    void updateCountdownLabels();
    void triggerDueReminders();
    void saveStoreBestEffort();
    void commitReminderChanges();
    void showAllCompletedDialog();
    void reAddCompletedReminder(const QString &completedId);
    void appendCompletedReminder(const Reminder &reminder);
    bool confirmShortRepeatInterval(int intervalSeconds, const QString &sourceLabel);

    void deleteReminderById(const QString &id);
    void editReminderById(const QString &id);
    void editChecklistById(const QString &id);
    void showNextQueuedPopup();

    void handlePopupOk(const QString &id);
    void handlePopupSnooze(const QString &id);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

    ReminderStore store;

    QLabel *titleLabel = nullptr;
    QLabel *nowLabel = nullptr;
    QPushButton *viewAllCompletedBtn = nullptr;

    QStackedWidget *stacked = nullptr;
    QLabel *emptyLabel = nullptr;
    QListWidget *list = nullptr;
    QLabel *completedHeaderLabel = nullptr;
    QListWidget *completedList = nullptr;

    QLineEdit *input = nullptr;

    QTimer *tickTimer = nullptr;

    QString activePopupId;
    QStringList queuedPopupIds;
    QHash<QString, QLabel *> countdownLabels;
    int currentGreetingPeriod = -1;
    bool saveErrorShown = false;
    bool quittingFromTray = false;
    bool trayHintShown = false;

    QSystemTrayIcon *trayIcon = nullptr;
    class QMenu *trayMenu = nullptr;
    QAction *showAction = nullptr;
    QAction *quitAction = nullptr;
};

}
