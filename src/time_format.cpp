#include "remindme/time_format.hpp"

namespace remindme
{

namespace
{
QString twoDigits(qint64 value)
{
    return QString("%1").arg(value, 2, 10, QChar('0'));
}
}

QString TimeFormat::formatCountdown(qint64 secondsRemaining)
{
    if (secondsRemaining < 0)
        secondsRemaining = 0;

    qint64 days = secondsRemaining / 86400;
    secondsRemaining %= 86400;
    qint64 hours = secondsRemaining / 3600;
    secondsRemaining %= 3600;
    qint64 minutes = secondsRemaining / 60;
    qint64 seconds = secondsRemaining % 60;

    if (days > 0)
    {
        return QString("%1d %2:%3:%4")
            .arg(days)
            .arg(twoDigits(hours))
            .arg(twoDigits(minutes))
            .arg(twoDigits(seconds));
    }

    return QString("%1:%2:%3")
        .arg(twoDigits(hours))
        .arg(twoDigits(minutes))
        .arg(twoDigits(seconds));
}

QString TimeFormat::formatDueDateTime(const QDateTime &dateTime)
{
    return dateTime.toString("M/d/yyyy  h:mm AP");
}

QString TimeFormat::formatClockTime(const QDateTime &dateTime)
{
    return dateTime.toString("h:mm:ss AP");
}

QString TimeFormat::formatOverdueText(qint64 overdueSeconds)
{
    return "Overdue " + formatCountdown(overdueSeconds);
}

QString TimeFormat::formatIntervalText(qint64 seconds)
{
    if (seconds <= 0)
        return "0 seconds";

    if (seconds % 86400 == 0)
    {
        const qint64 days = seconds / 86400;
        return (days == 1) ? "1 day" : QString("%1 days").arg(days);
    }

    if (seconds % 3600 == 0)
    {
        const qint64 hours = seconds / 3600;
        return (hours == 1) ? "1 hour" : QString("%1 hours").arg(hours);
    }

    if (seconds % 60 == 0)
    {
        const qint64 minutes = seconds / 60;
        return (minutes == 1) ? "1 minute" : QString("%1 minutes").arg(minutes);
    }

    return (seconds == 1) ? "1 second" : QString("%1 seconds").arg(seconds);
}

}
