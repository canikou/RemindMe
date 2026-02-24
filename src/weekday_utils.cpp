// SPDX-License-Identifier: MIT

#include "remindme/weekday_utils.hpp"

#include <QLocale>
#include <QStringList>

namespace remindme::WeekdayUtils
{

namespace
{

int rangeMaskInclusive(int startDay, int endDay)
{
    if (startDay < 1 || startDay > 7 || endDay < 1 || endDay > 7)
        return 0;

    int mask = 0;
    int day = startDay;
    for (int steps = 0; steps < 7; ++steps)
    {
        mask |= weekdayBit(day);
        if (day == endDay)
            break;
        day = (day == 7) ? 1 : (day + 1);
    }
    return mask;
}

bool isRangeConnector(const QString &token)
{
    return token == "-" || token == "to" || token == "through" || token == "thru";
}

}

int weekdayBit(int dayOfWeek)
{
    if (dayOfWeek < 1 || dayOfWeek > 7)
        return 0;
    return 1 << (dayOfWeek - 1);
}

int normalizeMask(int rawMask)
{
    return rawMask & kAllMask;
}

bool isSelected(int weekdayMask, int dayOfWeek)
{
    const int mask = normalizeMask(weekdayMask);
    if (mask == 0)
        return true;
    return (mask & weekdayBit(dayOfWeek)) != 0;
}

int weekdayFromToken(QString token)
{
    token = token.trimmed().toLower();
    while (!token.isEmpty() && (token.endsWith('.') || token.endsWith(',')))
        token.chop(1);

    if (token.endsWith('s') && token.size() > 3)
        token.chop(1);

    if (token == "mon" || token == "monday")
        return 1;
    if (token == "tue" || token == "tues" || token == "tuesday")
        return 2;
    if (token == "wed" || token == "weds" || token == "wednesday")
        return 3;
    if (token == "thu" || token == "thur" || token == "thurs" || token == "thursday")
        return 4;
    if (token == "fri" || token == "friday")
        return 5;
    if (token == "sat" || token == "saturday")
        return 6;
    if (token == "sun" || token == "sunday")
        return 7;
    return 0;
}

QString maskDisplayText(int weekdayMask)
{
    const int mask = normalizeMask(weekdayMask);
    if (mask == 0 || mask == kAllMask)
        return "daily";
    if (mask == kWeekdaysMask)
        return "weekdays";
    if (mask == kWeekendsMask)
        return "weekends";

    const QLocale locale;
    QStringList selectedDays;
    for (int day = 1; day <= 7; ++day)
    {
        if ((mask & weekdayBit(day)) == 0)
            continue;
        selectedDays.push_back(locale.dayName(day, QLocale::ShortFormat));
    }
    return selectedDays.join(", ");
}

bool parseWeekdayMaskSpec(const QString &repeatSpec, int &outMask, QString &outError)
{
    QString normalized = repeatSpec.toLower();
    normalized.replace(QChar(0x2013), "-");
    normalized.replace(QChar(0x2014), "-");
    normalized.replace(QChar(0x2212), "-");
    normalized.replace(",", " ");
    normalized.replace("/", " ");
    normalized.replace("&", " ");
    normalized.replace("+", " ");
    normalized.replace(".", " ");
    normalized = normalized.simplified();

    QStringList tokens = normalized.split(' ', Qt::SkipEmptyParts);
    if (tokens.isEmpty())
    {
        outError = "Repeat weekdays are empty.";
        return false;
    }

    int mask = 0;
    for (int i = 0; i < tokens.size(); ++i)
    {
        QString token = tokens[i].trimmed();
        if (token.isEmpty() ||
            token == "on" ||
            token == "and" ||
            token == "each" ||
            token == "every" ||
            token == "the" ||
            token == "etc" ||
            token == "from")
        {
            continue;
        }

        if (token == "weekday" || token == "weekdays")
        {
            mask |= kWeekdaysMask;
            continue;
        }

        if (token == "weekend" || token == "weekends")
        {
            mask |= kWeekendsMask;
            continue;
        }

        const int hyphenPos = token.indexOf('-');
        if (hyphenPos > 0 && hyphenPos < token.size() - 1)
        {
            const QString startToken = token.left(hyphenPos);
            const QString endToken = token.mid(hyphenPos + 1);
            const int startDay = weekdayFromToken(startToken);
            const int endDay = weekdayFromToken(endToken);
            if (startDay == 0 || endDay == 0)
            {
                outError = QString("Unrecognized weekday range \"%1\".").arg(token);
                return false;
            }

            mask |= rangeMaskInclusive(startDay, endDay);
            continue;
        }

        const int day = weekdayFromToken(token);
        if (day == 0)
        {
            outError = QString("Unrecognized weekday \"%1\". Use names like Monday, Wed, or Fri.").arg(token);
            return false;
        }

        if ((i + 2) < tokens.size() && isRangeConnector(tokens[i + 1]))
        {
            const int endDay = weekdayFromToken(tokens[i + 2]);
            if (endDay == 0)
            {
                outError = QString("Unrecognized weekday \"%1\" in range.").arg(tokens[i + 2]);
                return false;
            }

            mask |= rangeMaskInclusive(day, endDay);
            i += 2;
            continue;
        }

        mask |= weekdayBit(day);
    }

    mask = normalizeMask(mask);
    if (mask == 0)
    {
        outError = "Repeat weekdays are empty.";
        return false;
    }

    outMask = mask;
    return true;
}

}
