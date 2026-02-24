#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QListWidget>
#include <QHash>
#include <QPointer>
#include <QPoint>
#include <QStringList>
#include <QSystemTrayIcon>

#include "remindme/reminder_store.hpp"

class QAction;
class QPropertyAnimation;
class QToolButton;
class QVBoxLayout;
namespace remindme
{

class ReminderPopup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    void updateOverlayContents();
    void setOverlayVisible(bool visible);
    void triggerDueReminders();
    void queueNextPopupDisplay();
    void saveStoreBestEffort();
    void commitReminderChanges();
    void showAllCompletedDialog();
    void reAddCompletedReminder(const QString &completedId);
    void appendCompletedReminder(const Reminder &reminder);
    bool confirmShortRepeatInterval(int intervalSeconds, const QString &sourceLabel);
    void toggleCompletedPreview();
    void setCompletedPreviewCollapsed(bool collapsed, bool animate);
    int completedPreviewExpandedHeight() const;

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
    QCheckBox *overlayToggle = nullptr;

    QStackedWidget *stacked = nullptr;
    QLabel *emptyLabel = nullptr;
    QListWidget *list = nullptr;
    QWidget *completedSection = nullptr;
    QLabel *completedHeaderLabel = nullptr;
    QToolButton *completedToggleBtn = nullptr;
    QWidget *completedPreviewBody = nullptr;
    QPropertyAnimation *completedPreviewAnim = nullptr;
    QListWidget *completedList = nullptr;

    QLineEdit *input = nullptr;

    QTimer *tickTimer = nullptr;
    QWidget *overlayWindow = nullptr;
    QWidget *overlayBody = nullptr;
    QVBoxLayout *overlayRowsLayout = nullptr;

    QString activePopupId;
    QPointer<ReminderPopup> activePopup;
    QStringList queuedPopupIds;
    QHash<QString, QLabel *> countdownLabels;
    int currentGreetingPeriod = -1;
    bool saveErrorShown = false;
    bool overlayVisible = false;
    bool popupAdvanceQueued = false;
    bool quittingFromTray = false;
    bool trayHintShown = false;
    bool completedPreviewCollapsed = false;
    bool overlayDragging = false;
    QPoint overlayDragOffset;

    QSystemTrayIcon *trayIcon = nullptr;
    class QMenu *trayMenu = nullptr;
    QAction *showAction = nullptr;
    QAction *quitAction = nullptr;
};

}
