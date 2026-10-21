#include "remindme/reminder_popup.hpp"
#include "remindme/time_format.hpp"
#include "remindme/win_focus.hpp"

#include <QCloseEvent>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace remindme
{

namespace
{
constexpr int kTickIntervalMs = 1000;
constexpr int kOverdueDisplayThresholdSeconds = 60;
}

ReminderPopup::ReminderPopup(QString reminderId, QString title, QDateTime dueLocal, QWidget *parent)
    : QDialog(parent), m_id(std::move(reminderId)), m_title(std::move(title)), m_dueLocal(std::move(dueLocal))
{
    setObjectName("reminderPopup");
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowTitle(m_title);
    setModal(false);
    resize(560, 330);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // The board card: the one loud moment in the app.
    auto *boardCard = new QFrame();
    boardCard->setObjectName("popupBoard");
    auto *boardLayout = new QVBoxLayout(boardCard);
    boardLayout->setContentsMargins(28, 22, 28, 24);
    boardLayout->setSpacing(6);

    titleLabel = new QLabel(m_title);
    titleLabel->setProperty("role", "popupTitle");
    titleLabel->setWordWrap(true);

    const bool dueToday = m_dueLocal.date() == QDate::currentDate();
    auto *dueLabel = new QLabel(dueToday ? m_dueLocal.toString("'Due today at' h:mm AP")
                                         : m_dueLocal.toString("'Due' ddd, MMM d 'at' h:mm AP"));
    dueLabel->setProperty("role", "boardMuted");

    boardLayout->addWidget(titleLabel);
    boardLayout->addWidget(dueLabel);
    root->addWidget(boardCard);

    auto *body = new QVBoxLayout();
    body->setContentsMargins(28, 18, 28, 22);
    body->setSpacing(8);

    // Lead with the due state; the clock is secondary.
    overdueLabel = new QLabel();
    overdueLabel->setProperty("role", "popupDue");

    timeLabel = new QLabel();
    timeLabel->setProperty("role", "popupClock");

    body->addWidget(overdueLabel);
    body->addWidget(timeLabel);
    body->addStretch(1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    snoozeBtn = new QPushButton("Snooze 5 min");
    snoozeBtn->setToolTip("Remind me again in 5 minutes (Esc)");
    okBtn = new QPushButton("Done");
    okBtn->setObjectName("primaryButton");
    okBtn->setDefault(true);
    btnRow->addStretch(1);
    btnRow->addWidget(snoozeBtn);
    btnRow->addWidget(okBtn);
    body->addLayout(btnRow);
    root->addLayout(body, 1);

    connect(okBtn, &QPushButton::clicked, this, &ReminderPopup::onOk);
    connect(snoozeBtn, &QPushButton::clicked, this, &ReminderPopup::onSnooze);

    tick = new QTimer(this);
    tick->setInterval(kTickIntervalMs);
    connect(tick, &QTimer::timeout, this, &ReminderPopup::onTick);
    tick->start();

    onTick();
    okBtn->setFocus();
    WinFocus::bringToFront(this);
}

void ReminderPopup::onTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    timeLabel->setText(QString("It's %1").arg(now.toString("h:mm AP")));

    const qint64 overdueSeconds = m_dueLocal.secsTo(now);
    const qint64 overdueMinutes = overdueSeconds / 60;
    if (overdueSeconds < kOverdueDisplayThresholdSeconds)
        overdueLabel->setText("Due now");
    else if (overdueMinutes < 60)
        overdueLabel->setText(QString("Due %1 min ago").arg(overdueMinutes));
    else
        overdueLabel->setText(QString("Due %1 h %2 min ago").arg(overdueMinutes / 60).arg(overdueMinutes % 60));

    updateWindowTitle(overdueSeconds);
}

void ReminderPopup::onOk()
{
    m_actionHandled = true;
    emit okPressed(m_id);
    close();
}

void ReminderPopup::onSnooze()
{
    m_actionHandled = true;
    emit snoozePressed(m_id);
    close();
}

void ReminderPopup::closeEvent(QCloseEvent *event)
{
    if (!m_actionHandled)
    {
        m_actionHandled = true;
        emit okPressed(m_id);
    }
    QDialog::closeEvent(event);
}

// Esc goes through reject(), which hides without a closeEvent; snooze so the reminder is never left stuck.
void ReminderPopup::reject()
{
    // QDialog::closeEvent() calls reject() too, so only snooze when nothing was handled yet.
    if (!m_actionHandled)
    {
        onSnooze();
        return;
    }
    QDialog::reject();
}

void ReminderPopup::updateWindowTitle(qint64 overdueSeconds)
{
    QString title = m_title + " | " + TimeFormat::formatDueDateTime(m_dueLocal);
    if (overdueSeconds >= kOverdueDisplayThresholdSeconds)
        title += " | " + TimeFormat::formatOverdueText(overdueSeconds);

    setWindowTitle(title);
}

}
