#pragma once
#include <QDialog>
#include <QDateTime>

class QLabel;
class QPushButton;
class QTimer;

class ReminderPopup : public QDialog
{
    Q_OBJECT

public:
    ReminderPopup(QString reminderId,
                  QString title,
                  QDateTime dueLocal,
                  QWidget *parent = nullptr);

signals:
    void okPressed(const QString &reminderId);
    void snoozePressed(const QString &reminderId);

private slots:
    void onTick();
    void onOk();
    void onSnooze();

private:
    void updateWindowTitle(qint64 overdueSeconds);

    QString m_id;
    QString m_title;
    QDateTime m_dueLocal;

    QLabel *timeLabel = nullptr;
    QLabel *overdueLabel = nullptr;
    QLabel *titleLabel = nullptr;

    QPushButton *snoozeBtn = nullptr;
    QPushButton *okBtn = nullptr;

    QTimer *tick = nullptr;
};
