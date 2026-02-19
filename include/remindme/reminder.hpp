#pragma once
#include <QString>
#include <QDateTime>
#include <QTime>

namespace remindme
{

enum class ScheduleType
{
    Relative,   // "in 5h23m"
    AtTimeOfDay // "at 7:00AM"
};

struct Reminder
{
    QString id; // stable ID for edit/delete
    QString title;

    QDateTime nextLocal; // next fire time (local time)

    ScheduleType scheduleType = ScheduleType::Relative;

    // For "in ..."
    int intervalSeconds = 0; // also used for repeating relative reminders

    // For "at ..."
    QTime timeOfDay; // used for daily repeating (and display)

    bool repeating = false;
};

struct CompletedReminder
{
    QString id;
    QString title;
    ScheduleType scheduleType = ScheduleType::Relative;
    int intervalSeconds = 0;
    QTime timeOfDay;
    QDateTime completedAt;
    int completionCount = 1;
};

}
