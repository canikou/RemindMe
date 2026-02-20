#include "remindme/parser.hpp"

#include <QRegularExpression>
#include <QStringView>
#include <climits>
#include <limits>

namespace remindme
{

namespace
{
QString trimmed(const QString &s)
{
    return s.trimmed();
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

const QRegularExpression &repeatDirectiveRe()
{
    static const QRegularExpression re(
        R"(^(.*)\bevery\b\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &scheduleKeywordRe()
{
    static const QRegularExpression re(
        R"(\b(in|at)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &durationTokenRe()
{
    static const QRegularExpression re(
        R"(([\d\+\-\*\/\(\)\s]+)\s*(d|day|days|h|hr|hrs|hour|hours|m|min|mins|minute|minutes|s|sec|secs|second|seconds)\b)");
    return re;
}

const QRegularExpression &timeOfDayRe()
{
    static const QRegularExpression re(
        R"(^(\d{1,2})(?::(\d{2}))?([AaPp][Mm])?$)");
    return re;
}

bool checkedAdd(qlonglong lhs, qlonglong rhs, qlonglong &out)
{
    if ((rhs > 0 && lhs > std::numeric_limits<qlonglong>::max() - rhs) ||
        (rhs < 0 && lhs < std::numeric_limits<qlonglong>::min() - rhs))
    {
        return false;
    }

    out = lhs + rhs;
    return true;
}

bool checkedSub(qlonglong lhs, qlonglong rhs, qlonglong &out)
{
    if ((rhs > 0 && lhs < std::numeric_limits<qlonglong>::min() + rhs) ||
        (rhs < 0 && lhs > std::numeric_limits<qlonglong>::max() + rhs))
    {
        return false;
    }

    out = lhs - rhs;
    return true;
}

bool checkedMul(qlonglong lhs, qlonglong rhs, qlonglong &out)
{
    if (lhs == 0 || rhs == 0)
    {
        out = 0;
        return true;
    }

    if (lhs > 0)
    {
        if (rhs > 0)
        {
            if (lhs > std::numeric_limits<qlonglong>::max() / rhs)
                return false;
        }
        else
        {
            if (rhs < std::numeric_limits<qlonglong>::min() / lhs)
                return false;
        }
    }
    else
    {
        if (rhs > 0)
        {
            if (lhs < std::numeric_limits<qlonglong>::min() / rhs)
                return false;
        }
        else
        {
            if (lhs != 0 && rhs < std::numeric_limits<qlonglong>::max() / lhs)
                return false;
        }
    }

    out = lhs * rhs;
    return true;
}

class IntExpressionParser
{
public:
    explicit IntExpressionParser(QStringView expression)
        : m_expression(expression)
    {
    }

    bool parse(qlonglong &outValue, QString &outError)
    {
        if (!parseExpression(outValue))
        {
            outError = m_error.isEmpty() ? "Invalid math expression." : m_error;
            return false;
        }

        skipSpaces();
        if (!atEnd())
        {
            setError(QString("Unexpected token in math expression near \"%1\".")
                         .arg(QString(m_expression.mid(m_pos, 1))));
            outError = m_error;
            return false;
        }

        return true;
    }

private:
    bool parseExpression(qlonglong &outValue)
    {
        if (!parseTerm(outValue))
            return false;

        while (true)
        {
            skipSpaces();
            if (atEnd())
                return true;

            const QChar op = m_expression.at(m_pos);
            if (op != '+' && op != '-')
                return true;
            ++m_pos;

            qlonglong rhs = 0;
            if (!parseTerm(rhs))
                return false;

            qlonglong combined = 0;
            const bool ok = (op == '+') ? checkedAdd(outValue, rhs, combined) : checkedSub(outValue, rhs, combined);
            if (!ok)
            {
                setError("Math expression overflowed.");
                return false;
            }

            outValue = combined;
        }
    }

    bool parseTerm(qlonglong &outValue)
    {
        if (!parseFactor(outValue))
            return false;

        while (true)
        {
            skipSpaces();
            if (atEnd())
                return true;

            const QChar op = m_expression.at(m_pos);
            if (op != '*' && op != '/')
                return true;
            ++m_pos;

            qlonglong rhs = 0;
            if (!parseFactor(rhs))
                return false;

            qlonglong combined = 0;
            if (op == '*')
            {
                if (!checkedMul(outValue, rhs, combined))
                {
                    setError("Math expression overflowed.");
                    return false;
                }
            }
            else
            {
                if (rhs == 0)
                {
                    setError("Division by zero in math expression.");
                    return false;
                }
                if (outValue % rhs != 0)
                {
                    setError("Math expression must resolve to a whole number.");
                    return false;
                }
                combined = outValue / rhs;
            }

            outValue = combined;
        }
    }

    bool parseFactor(qlonglong &outValue)
    {
        skipSpaces();
        if (atEnd())
        {
            setError("Math expression ended unexpectedly.");
            return false;
        }

        const QChar c = m_expression.at(m_pos);
        if (c == '+' || c == '-')
        {
            ++m_pos;
            if (!parseFactor(outValue))
                return false;

            if (c == '-')
            {
                if (outValue == std::numeric_limits<qlonglong>::min())
                {
                    setError("Math expression overflowed.");
                    return false;
                }
                outValue = -outValue;
            }
            return true;
        }

        if (c == '(')
        {
            ++m_pos;
            if (!parseExpression(outValue))
                return false;

            skipSpaces();
            if (atEnd() || m_expression.at(m_pos) != ')')
            {
                setError("Missing ')' in math expression.");
                return false;
            }
            ++m_pos;
            return true;
        }

        return parseNumber(outValue);
    }

    bool parseNumber(qlonglong &outValue)
    {
        skipSpaces();

        int start = m_pos;
        while (!atEnd() && m_expression.at(m_pos).isDigit())
            ++m_pos;

        if (start == m_pos)
        {
            setError("Expected a number in math expression.");
            return false;
        }

        bool ok = false;
        const qlonglong parsed = QString(m_expression.mid(start, m_pos - start)).toLongLong(&ok);
        if (!ok)
        {
            setError("Number in math expression is out of range.");
            return false;
        }

        outValue = parsed;
        return true;
    }

    void skipSpaces()
    {
        while (!atEnd() && m_expression.at(m_pos).isSpace())
            ++m_pos;
    }

    bool atEnd() const
    {
        return m_pos >= m_expression.size();
    }

    void setError(const QString &message)
    {
        if (m_error.isEmpty())
            m_error = message;
    }

    QStringView m_expression;
    int m_pos = 0;
    QString m_error;
};

bool parseDurationAmount(const QString &rawAmount, qlonglong &outAmount, QString &outError)
{
    const QString amount = rawAmount.trimmed();
    if (amount.isEmpty())
    {
        outError = "Duration amount is empty.";
        return false;
    }

    IntExpressionParser parser{QStringView(amount)};
    if (!parser.parse(outAmount, outError))
        return false;

    if (outAmount <= 0)
    {
        outError = "Duration amounts must be > 0.";
        return false;
    }

    return true;
}

bool parseDurationDelta(qlonglong amount, const QString &unit, qlonglong &outDelta)
{
    if (unit == "d" || unit == "day" || unit == "days")
        return checkedMul(amount, 86400LL, outDelta);

    if (unit == "h" || unit == "hr" || unit == "hrs" || unit == "hour" || unit == "hours")
        return checkedMul(amount, 3600LL, outDelta);

    if (unit == "m" || unit == "min" || unit == "mins" || unit == "minute" || unit == "minutes")
        return checkedMul(amount, 60LL, outDelta);

    if (unit == "s" || unit == "sec" || unit == "secs" || unit == "second" || unit == "seconds")
    {
        outDelta = amount;
        return true;
    }

    outDelta = 0;
    return true;
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

    const QRegularExpressionMatch repeatMatch = repeatDirectiveRe().match(s);
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

    QRegularExpressionMatchIterator it = scheduleKeywordRe().globalMatch(expression);

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

    QRegularExpressionMatchIterator it = durationTokenRe().globalMatch(x);

    qlonglong total = 0;
    int matches = 0;

    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        qlonglong n = 0;
        if (!parseDurationAmount(m.captured(1), n, outError))
            return false;

        const QString u = m.captured(2);

        qlonglong delta = 0;
        parseDurationDelta(n, u, delta);

        if (!checkedAdd(total, delta, total))
        {
            outError = "Duration is too large.";
            return false;
        }

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

    const QRegularExpressionMatch m = timeOfDayRe().match(x);

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
