#pragma once
#include <QString>
#include <QDateTime>
#include <QTime>
#include <QVector>

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
    int repeatWeekdaysMask = 0; // bit mask for Monday..Sunday when repeating at a specific day

    bool repeating = false;

    struct ChecklistItem
    {
        QString text;
        bool checked = false;
    };

    QVector<ChecklistItem> checklistItems;

    void resetChecklist()
    {
        for (ChecklistItem &item : checklistItems)
            item.checked = false;
    }

    int checkedChecklistCount() const
    {
        int count = 0;
        for (const ChecklistItem &item : checklistItems)
        {
            if (item.checked)
                ++count;
        }
        return count;
    }

    void enforceChecklistConstraints()
    {
        if (!repeating)
            checklistItems.clear();
    }
};

struct CompletedReminder
{
    QString id;
    QString title;
    ScheduleType scheduleType = ScheduleType::Relative;
    int intervalSeconds = 0;
    QTime timeOfDay;
    int repeatWeekdaysMask = 0;
    QDateTime completedAt;
    int completionCount = 1;
};

}
