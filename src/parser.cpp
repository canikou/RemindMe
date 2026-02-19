#include "remindme/parser.hpp"

#include <QRegularExpression>
#include <climits>

namespace remindme
{

namespace
{
QString trimmed(const QString &s)
{
    QString out = s;
    return out.trimmed();
}

bool isDayKeyword(const QString &s)
{
    const QString x = s.trimmed().toLower();
    return x == "day" || x == "daily" || x == "everyday";
}

bool isHourKeyword(const QString &s)
{
    const QString x = s.trimmed().toLower();
    return x == "hour" || x == "hours" || x == "hr" || x == "hrs";
}

bool isMinuteKeyword(const QString &s)
{
    const QString x = s.trimmed().toLower();
    return x == "minute" || x == "minutes" || x == "min" || x == "mins";
}

bool containsSecondUnits(const QString &s)
{
    static const QRegularExpression secondRe(
        R"(\b(s|sec|secs|second|seconds)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return secondRe.match(s).hasMatch();
}
}

ParseResult Parser::parseInput(const QString &input)
{
    ParseResult r;
    const QString s = trimmed(input);

    if (s.isEmpty())
    {
        r.ok = false;
        r.error = "Type something like: \"Drink water in 45m\" or \"Stand up at 7:00AM\"";
        return r;
    }

    QString expression = s;
    QString repeatSpec;

    const QRegularExpression repeatRe(R"(^(.*)\bevery\b\s+(.+)$)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch repeatMatch = repeatRe.match(s);
    if (repeatMatch.hasMatch())
    {
        expression = trimmed(repeatMatch.captured(1));
        repeatSpec = trimmed(repeatMatch.captured(2));

        if (expression.isEmpty() || repeatSpec.isEmpty())
        {
            r.ok = false;
            r.error = "Invalid repeating format. Example: \"Take break at 7:00AM every day\"";
            return r;
        }
    }

    QRegularExpression re(R"(\b(in|at)\b)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(expression);

    int lastPos = -1;
    int lastLen = 0;
    QString lastKw;

    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        lastPos = m.capturedStart();
        lastLen = m.capturedLength();
        lastKw = m.captured(1).toLower();
    }

    if (lastPos < 0)
    {
        r.ok = false;
        r.error = "Missing keyword. Use \"... in ...\" or \"... at ...\"";
        return r;
    }

    const QString left = trimmed(expression.left(lastPos));
    const QString right = trimmed(expression.mid(lastPos + lastLen));

    if (left.isEmpty())
    {
        r.ok = false;
        r.error = "Reminder name is empty. Put text before \"in\" or \"at\".";
        return r;
    }
    if (right.isEmpty())
    {
        r.ok = false;
        r.error = "Time part is empty. Put time after \"in\" or \"at\".";
        return r;
    }

    r.title = left;

    if (lastKw == "in")
    {
        r.isRelative = true;
        int sec = 0;
        QString err;
        if (!parseDurationToSeconds(right, sec, err))
        {
            r.ok = false;
            r.error = err;
            return r;
        }
        r.durationSeconds = sec;
    }
    else
    {
        r.isRelative = false;
        QTime t;
        QString err;
        if (!parseTimeOfDay(right, t, err))
        {
            r.ok = false;
            r.error = err;
            return r;
        }
        r.timeOfDay = t;
    }

    if (!repeatSpec.isEmpty())
    {
        int repeatSeconds = 0;
        QString err;
        if (!parseRepeatIntervalToSeconds(repeatSpec, repeatSeconds, err))
        {
            r.ok = false;
            r.error = err;
            return r;
        }

        r.hasRepeatDirective = true;
        r.repeatIntervalSeconds = repeatSeconds;
    }

    r.ok = true;
    return r;
}

bool Parser::parseDurationToSeconds(const QString &s, int &outSeconds, QString &outError)
{
    QString x = s.toLower();
    x.replace(",", " ");
    x.replace("and", " ");

    QRegularExpression token(
        R"((\d+)\s*(d|day|days|h|hr|hrs|hour|hours|m|min|mins|minute|minutes|s|sec|secs|second|seconds)\b)");

    QRegularExpressionMatchIterator it = token.globalMatch(x);

    long long total = 0;
    int matches = 0;

    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const long long n = m.captured(1).toLongLong();
        const QString u = m.captured(2);

        if (u == "d" || u == "day" || u == "days")
            total += n * 86400LL;
        else if (u == "h" || u == "hr" || u == "hrs" || u == "hour" || u == "hours")
            total += n * 3600LL;
        else if (u == "m" || u == "min" || u == "mins" || u == "minute" || u == "minutes")
            total += n * 60LL;
        else if (u == "s" || u == "sec" || u == "secs" || u == "second" || u == "seconds")
            total += n;

        ++matches;
    }

    if (matches == 0)
    {
        outError = "Couldn't parse duration. Examples: \"45m\", \"5h23m\", \"2 days 3 hours\"";
        return false;
    }
    if (total <= 0)
    {
        outError = "Duration must be > 0.";
        return false;
    }
    if (total > INT_MAX)
    {
        outError = "Duration is too large.";
        return false;
    }

    outSeconds = static_cast<int>(total);
    return true;
}

bool Parser::parseTimeOfDay(const QString &s, QTime &outTime, QString &outError)
{
    QString x = s.trimmed();
    x.replace(" ", "");

    QRegularExpression re(R"(^(\d{1,2})(?::(\d{2}))?([AaPp][Mm])?$)");
    const QRegularExpressionMatch m = re.match(x);

    if (!m.hasMatch())
    {
        outError = "Couldn't parse time. Examples: \"7:00AM\", \"7am\", \"19:30\"";
        return false;
    }

    int hour = m.captured(1).toInt();
    const int minute = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
    const QString ampm = m.captured(3).toLower();

    if (minute < 0 || minute > 59)
    {
        outError = "Minutes must be 00-59.";
        return false;
    }

    if (!ampm.isEmpty())
    {
        if (hour < 1 || hour > 12)
        {
            outError = "For AM/PM time, hour must be 1-12.";
            return false;
        }
        if (ampm == "am")
        {
            if (hour == 12)
                hour = 0;
        }
        else if (hour != 12)
        {
            hour += 12;
        }
    }
    else if (hour < 0 || hour > 23)
    {
        outError = "Hour must be 0-23 (or use AM/PM).";
        return false;
    }

    outTime = QTime(hour, minute, 0);
    return outTime.isValid();
}

bool Parser::parseRepeatIntervalToSeconds(const QString &s, int &outSeconds, QString &outError)
{
    if (s.trimmed().isEmpty())
    {
        outError = "Repeat interval is empty.";
        return false;
    }

    if (isDayKeyword(s))
    {
        outSeconds = 86400;
        return true;
    }
    if (isHourKeyword(s))
    {
        outSeconds = 3600;
        return true;
    }
    if (isMinuteKeyword(s))
    {
        outSeconds = 60;
        return true;
    }

    if (containsSecondUnits(s))
    {
        outError = "Repeat interval supports minutes, hours, or days.";
        return false;
    }

    int seconds = 0;
    QString err;
    if (!parseDurationToSeconds(s, seconds, err))
    {
        outError = err;
        return false;
    }
    if (seconds < 60)
    {
        outError = "Repeat interval supports minutes, hours, or days.";
        return false;
    }

    outSeconds = seconds;
    return true;
}

}
