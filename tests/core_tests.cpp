// SPDX-License-Identifier: MIT

#include "remindme/parser.hpp"
#include "remindme/reminder_store.hpp"

#include <QDateTime>
#include <QTime>

#include <iostream>

namespace
{
using remindme::Reminder;

const Reminder *findReminderByTitle(const remindme::ReminderStore &store, const QString &title)
{
    for (const Reminder &reminder : store.items())
    {
        if (reminder.title == title)
            return &reminder;
    }
    return nullptr;
}

bool testParseRelative()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Drink water in 45m");
    return r.ok && r.isRelative && r.title == "Drink water" && r.durationSeconds == 45 * 60;
}

bool testParseAtTime()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Stand up at 7:00AM");
    return r.ok && !r.isRelative && r.title == "Stand up" && r.timeOfDay.hour() == 7 && r.timeOfDay.minute() == 0;
}

bool testParseRepeat()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Hydrate in 10m every 2 hours");
    return r.ok && r.hasRepeatDirective && r.repeatIntervalSeconds == 2 * 3600;
}

bool testParseMathDuration()
{
    int seconds = 0;
    QString err;
    if (!remindme::Parser::parseDurationToSeconds("(3*10) minutes", seconds, err))
        return false;
    return seconds == 30 * 60;
}

bool testParseMathInput()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Drink water in (3*10) minutes");
    return r.ok && r.isRelative && r.durationSeconds == 30 * 60;
}

bool testParseMathRepeatInterval()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Hydrate in 10m every (2+1) hours");
    return r.ok && r.hasRepeatDirective && r.repeatIntervalSeconds == 3 * 3600;
}

bool testRejectInvalidMathDuration()
{
    int seconds = 0;
    QString err;
    const bool ok = remindme::Parser::parseDurationToSeconds("(10/0) minutes", seconds, err);
    return !ok && !err.isEmpty();
}

bool testChecklistReset()
{
    Reminder reminder;
    reminder.repeating = true;
    reminder.checklistItems = {
        {"First", true},
        {"Second", false},
        {"Third", true},
    };

    reminder.resetChecklist();
    if (reminder.checkedChecklistCount() != 0)
        return false;

    for (const Reminder::ChecklistItem &item : reminder.checklistItems)
    {
        if (item.checked)
            return false;
    }

    reminder.repeating = false;
    reminder.enforceChecklistConstraints();
    return reminder.checklistItems.isEmpty();
}

bool testShareExportImportRoundTrip()
{
    remindme::ReminderStore sourceStore;

    Reminder repeating;
    repeating.id = "source-repeating-id";
    repeating.title = "Daily Cleanup";
    repeating.repeating = true;
    repeating.scheduleType = remindme::ScheduleType::Relative;
    repeating.intervalSeconds = 24 * 3600;
    repeating.nextLocal = QDateTime::currentDateTime().addSecs(3600);
    repeating.checklistItems = {
        {"Desk", true},
        {"Inbox", false},
    };
    sourceStore.items().push_back(repeating);

    Reminder oneShot;
    oneShot.id = "source-oneshot-id";
    oneShot.title = "Doctor Appointment";
    oneShot.repeating = false;
    oneShot.scheduleType = remindme::ScheduleType::AtTimeOfDay;
    oneShot.timeOfDay = QTime(14, 30);
    oneShot.nextLocal = QDateTime::currentDateTime().addDays(1);
    oneShot.checklistItems = {
        {"Should clear on import", true},
    };
    sourceStore.items().push_back(oneShot);

    QString err;
    const QString share = sourceStore.exportShareString(err);
    if (share.isEmpty() || !err.isEmpty())
        return false;

    remindme::ReminderStore importedStore;
    int importedCount = 0;
    if (!importedStore.importShareString(share, importedCount, err))
        return false;
    if (importedCount != 2)
        return false;

    const Reminder *importedRepeating = findReminderByTitle(importedStore, "Daily Cleanup");
    const Reminder *importedOneShot = findReminderByTitle(importedStore, "Doctor Appointment");
    if (!importedRepeating || !importedOneShot)
        return false;

    if (!importedRepeating->repeating || importedRepeating->checklistItems.size() != 2)
        return false;
    if (importedRepeating->checkedChecklistCount() != 1)
        return false;

    if (importedOneShot->repeating)
        return false;
    if (!importedOneShot->checklistItems.isEmpty())
        return false;

    return importedStore.importShareString("bad-share-string", importedCount, err) == false;
}
}

int main()
{
    if (!testParseRelative())
    {
        std::cerr << "testParseRelative failed\n";
        return 1;
    }
    if (!testParseAtTime())
    {
        std::cerr << "testParseAtTime failed\n";
        return 1;
    }
    if (!testParseRepeat())
    {
        std::cerr << "testParseRepeat failed\n";
        return 1;
    }
    if (!testParseMathDuration())
    {
        std::cerr << "testParseMathDuration failed\n";
        return 1;
    }
    if (!testParseMathInput())
    {
        std::cerr << "testParseMathInput failed\n";
        return 1;
    }
    if (!testParseMathRepeatInterval())
    {
        std::cerr << "testParseMathRepeatInterval failed\n";
        return 1;
    }
    if (!testRejectInvalidMathDuration())
    {
        std::cerr << "testRejectInvalidMathDuration failed\n";
        return 1;
    }
    if (!testChecklistReset())
    {
        std::cerr << "testChecklistReset failed\n";
        return 1;
    }
    if (!testShareExportImportRoundTrip())
    {
        std::cerr << "testShareExportImportRoundTrip failed\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
