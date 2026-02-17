#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QListWidget>
#include <QSet>
#include <QHash>

#include "ReminderStore.h"

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

private:
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

    void handlePopupOk(const QString &id);
    void handlePopupSnooze(const QString &id);

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

    QSet<QString> activePopups;
    QHash<QString, QLabel *> countdownLabels;
    int currentGreetingPeriod = -1;
    bool saveErrorShown = false;
};
