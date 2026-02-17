#pragma once

#include <QDateTime>
#include <QString>

namespace TimeFormat
{
QString formatCountdown(qint64 secondsRemaining);
QString formatDueDateTime(const QDateTime &dateTime);
QString formatClockTime(const QDateTime &dateTime);
QString formatOverdueText(qint64 overdueSeconds);
QString formatIntervalText(qint64 seconds);
}
