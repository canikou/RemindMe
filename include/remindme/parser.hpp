#pragma once
#include <QDate>
#include <QString>
#include <QTime>

namespace remindme
{

struct ParseResult
{
    bool ok = false;
    QString error;

    QString title;

    bool isRelative = true;  // true = "in", false = "at"
    int durationSeconds = 0; // for "in"
    QTime timeOfDay;         // for "at"/"on"
    QDate date;              // for "on <date>"; invalid when absent

    bool hasRepeatDirective = false;
    int repeatIntervalSeconds = 0;
    int repeatWeekdaysMask = 0;
};

class Parser
{
public:
    static ParseResult parseInput(const QString &input);
    static bool parseDurationToSeconds(const QString &s, int &outSeconds, QString &outError);
    static bool parseTimeOfDay(const QString &s, QTime &outTime, QString &outError);
    static bool parseDateTime(const QString &s, QDate &outDate, QTime &outTime, QString &outError);
    static bool parseRepeatIntervalToSeconds(const QString &s, int &outSeconds, QString &outError);
};

}
