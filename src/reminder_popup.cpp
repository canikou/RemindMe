#include "remindme/reminder_popup.hpp"
#include "remindme/time_format.hpp"
#include "remindme/win_focus.hpp"

#include <QCloseEvent>
#include <QDateTime>
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
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowTitle(m_title);
    setModal(false);
    resize(640, 360);

    setStyleSheet(R"(
        QDialog { background: #2b2b2b; color: #eaeaea; }
        QLabel { color: #eaeaea; }
        QPushButton {
            background: #3a3a3a;
            border: 1px solid #5a5a5a;
            padding: 10px 18px;
            font-size: 16px;
        }
        QPushButton:focus { outline: none; }
        QPushButton:hover { background: #444444; }
    )");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(14);

    timeLabel = new QLabel();
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet("font-size: 42px; font-weight: 700;");
    root->addWidget(timeLabel);

    overdueLabel = new QLabel();
    overdueLabel->setAlignment(Qt::AlignCenter);
    overdueLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #ff4d4d;");
    overdueLabel->hide();
    root->addWidget(overdueLabel);

    titleLabel = new QLabel(m_title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 26px; font-weight: 600;");
    root->addWidget(titleLabel);

    root->addStretch(1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(14);

    snoozeBtn = new QPushButton("Snooze (5 min.)");
    okBtn = new QPushButton("OK");
    btnRow->addWidget(snoozeBtn, 1);
    btnRow->addWidget(okBtn, 1);
    root->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, this, &ReminderPopup::onOk);
    connect(snoozeBtn, &QPushButton::clicked, this, &ReminderPopup::onSnooze);

    tick = new QTimer(this);
    tick->setInterval(kTickIntervalMs);
    connect(tick, &QTimer::timeout, this, &ReminderPopup::onTick);
    tick->start();

    onTick();
    WinFocus::bringToFront(this);
}

void ReminderPopup::onTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    timeLabel->setText(TimeFormat::formatClockTime(now));

    const qint64 overdueSeconds = m_dueLocal.secsTo(now);
    if (overdueSeconds >= kOverdueDisplayThresholdSeconds)
    {
        overdueLabel->setText(TimeFormat::formatOverdueText(overdueSeconds));
        overdueLabel->show();
    }
    else
    {
        overdueLabel->hide();
    }

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

void ReminderPopup::updateWindowTitle(qint64 overdueSeconds)
{
    QString title = m_title + " | " + TimeFormat::formatDueDateTime(m_dueLocal);
    if (overdueSeconds >= kOverdueDisplayThresholdSeconds)
        title += " | " + TimeFormat::formatOverdueText(overdueSeconds);

    setWindowTitle(title);
}

}
