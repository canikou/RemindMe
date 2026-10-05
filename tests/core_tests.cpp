// SPDX-License-Identifier: MIT

#include "remindme/parser.hpp"
#include "remindme/reminder_store.hpp"
#include "remindme/update_utils.hpp"
#include "remindme/weekday_utils.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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

bool testParseTimeAmPmBoundaries()
{
    const remindme::ParseResult midnight = remindme::Parser::parseInput("Midnight task at 12:00AM");
    if (!midnight.ok || midnight.isRelative || midnight.timeOfDay != QTime(0, 0))
        return false;

    const remindme::ParseResult noon = remindme::Parser::parseInput("Noon task at 12:00PM");
    if (!noon.ok || noon.isRelative || noon.timeOfDay != QTime(12, 0))
        return false;

    const remindme::ParseResult invalid = remindme::Parser::parseInput("Invalid task at 00:30PM");
    return !invalid.ok && !invalid.error.isEmpty();
}

bool testParseRepeat()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Hydrate in 10m every 2 hours");
    return r.ok && r.hasRepeatDirective && r.repeatIntervalSeconds == 2 * 3600;
}

bool testParseRepeatWeekday()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Saturday");
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatIntervalSeconds == 0 &&
           r.repeatWeekdaysMask == remindme::WeekdayUtils::weekdayBit(6);
}

bool testParseRepeatWeekdayWithEtc()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Saturday, etc.");
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == remindme::WeekdayUtils::weekdayBit(6);
}

bool testParseRepeatWeekdaysAlias()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every weekdays");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekendsAlias()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every weekends");
    const int expectedMask = remindme::WeekdayUtils::weekdayBit(6) | remindme::WeekdayUtils::weekdayBit(7);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatMultipleWeekdays()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Mon Wed Fri");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayRangeDash()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Mon-Fri");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayRangeThrough()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Monday through Friday");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayRangeThru()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Tue thru Thu");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayFromTo()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every from monday to friday");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayRangeWrapAround()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Fri-Mon");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(5) |
        remindme::WeekdayUtils::weekdayBit(6) |
        remindme::WeekdayUtils::weekdayBit(7) |
        remindme::WeekdayUtils::weekdayBit(1);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayUnicodeDash()
{
    const QString input = QString::fromUtf8("Reminder at 7:00AM every Mon\u2013Fri");
    const remindme::ParseResult r = remindme::Parser::parseInput(input);
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(4) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testParseRepeatWeekdayPluralTokens()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Mondays and Fridays");
    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(5);
    return r.ok &&
           !r.isRelative &&
           r.hasRepeatDirective &&
           r.repeatWeekdaysMask == expectedMask;
}

bool testRejectWeekdayRepeatOnRelativeReminder()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder in 30m every Saturday");
    return !r.ok && !r.error.isEmpty();
}

bool testRejectMalformedWeekdayRange()
{
    const remindme::ParseResult r = remindme::Parser::parseInput("Reminder at 7:00AM every Mon-");
    return !r.ok && !r.error.isEmpty();
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

bool testRejectNonIntegralMathDuration()
{
    int seconds = 0;
    QString err;
    const bool ok = remindme::Parser::parseDurationToSeconds("(5/2) minutes", seconds, err);
    return !ok && !err.isEmpty();
}

bool testRejectOverflowDuration()
{
    int seconds = 0;
    QString err;
    const bool ok = remindme::Parser::parseDurationToSeconds("2147483648 seconds", seconds, err);
    return !ok && !err.isEmpty();
}

bool testRejectSecondRepeatInterval()
{
    int seconds = 0;
    QString err;
    const bool ok = remindme::Parser::parseRepeatIntervalToSeconds("30 seconds", seconds, err);
    return !ok && !err.isEmpty();
}

bool testParseDurationExpressionPrecedence()
{
    int seconds = 0;
    QString err;
    if (!remindme::Parser::parseDurationToSeconds("2+3*4 minutes", seconds, err))
        return false;
    if (seconds != 14 * 60)
        return false;

    if (!remindme::Parser::parseDurationToSeconds("(2+3)*4 minutes", seconds, err))
        return false;

    return seconds == 20 * 60;
}

bool testWeekdayMaskSpecMixedSeparators()
{
    int mask = 0;
    QString err;
    if (!remindme::WeekdayUtils::parseWeekdayMaskSpec("Mon, Wed/Fri+Sun", mask, err))
        return false;

    const int expectedMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(5) |
        remindme::WeekdayUtils::weekdayBit(7);
    return mask == expectedMask;
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

    Reminder weeklyAtTime;
    weeklyAtTime.id = "source-weekly-id";
    weeklyAtTime.title = "Plan Sprint";
    weeklyAtTime.repeating = true;
    weeklyAtTime.scheduleType = remindme::ScheduleType::AtTimeOfDay;
    weeklyAtTime.timeOfDay = QTime(8, 15);
    weeklyAtTime.repeatWeekdaysMask =
        remindme::WeekdayUtils::weekdayBit(1) |
        remindme::WeekdayUtils::weekdayBit(3) |
        remindme::WeekdayUtils::weekdayBit(5);
    weeklyAtTime.nextLocal = QDateTime::currentDateTime().addDays(1);
    sourceStore.items().push_back(weeklyAtTime);

    QString err;
    const QString share = sourceStore.exportShareString(err);
    if (share.isEmpty() || !err.isEmpty())
        return false;

    remindme::ReminderStore importedStore;
    int importedCount = 0;
    if (!importedStore.importShareString(share, importedCount, err))
        return false;
    if (importedCount != 3)
        return false;

    const Reminder *importedRepeating = findReminderByTitle(importedStore, "Daily Cleanup");
    const Reminder *importedOneShot = findReminderByTitle(importedStore, "Doctor Appointment");
    const Reminder *importedWeekly = findReminderByTitle(importedStore, "Plan Sprint");
    if (!importedRepeating || !importedOneShot || !importedWeekly)
        return false;

    if (!importedRepeating->repeating || importedRepeating->checklistItems.size() != 2)
        return false;
    if (importedRepeating->checkedChecklistCount() != 1)
        return false;

    if (importedOneShot->repeating)
        return false;
    if (!importedOneShot->checklistItems.isEmpty())
        return false;
    if (!importedWeekly->repeating)
        return false;
    if (importedWeekly->scheduleType != remindme::ScheduleType::AtTimeOfDay)
        return false;
    if (importedWeekly->repeatWeekdaysMask !=
        (remindme::WeekdayUtils::weekdayBit(1) |
         remindme::WeekdayUtils::weekdayBit(3) |
         remindme::WeekdayUtils::weekdayBit(5)))
        return false;

    return importedStore.importShareString("bad-share-string", importedCount, err) == false;
}

bool testStoreSaveLoadRoundTrip()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString storagePath = tempDir.filePath("documents/RemindMe/reminders.json");
    remindme::ReminderStore sourceStore(storagePath);

    Reminder repeating;
    repeating.id = "repeat-id";
    repeating.title = "Daily Standup";
    repeating.repeating = true;
    repeating.scheduleType = remindme::ScheduleType::Relative;
    repeating.intervalSeconds = 3600;
    repeating.nextLocal = QDateTime::currentDateTime().addSecs(300);
    repeating.checklistItems = {
        {"Ping team", true},
        {"Update notes", false},
    };
    sourceStore.items().push_back(repeating);

    Reminder oneShot;
    oneShot.id = "oneshot-id";
    oneShot.title = "Book dentist";
    oneShot.repeating = false;
    oneShot.scheduleType = remindme::ScheduleType::AtTimeOfDay;
    oneShot.timeOfDay = QTime(9, 45);
    oneShot.nextLocal = QDateTime::currentDateTime().addDays(1);
    sourceStore.items().push_back(oneShot);

    Reminder weeklyAtTime;
    weeklyAtTime.id = "weekly-id";
    weeklyAtTime.title = "Weekly Sync";
    weeklyAtTime.repeating = true;
    weeklyAtTime.scheduleType = remindme::ScheduleType::AtTimeOfDay;
    weeklyAtTime.timeOfDay = QTime(11, 0);
    weeklyAtTime.repeatWeekdaysMask =
        remindme::WeekdayUtils::weekdayBit(2) |
        remindme::WeekdayUtils::weekdayBit(4);
    weeklyAtTime.nextLocal = QDateTime::currentDateTime().addDays(1);
    sourceStore.items().push_back(weeklyAtTime);

    remindme::CompletedReminder completed;
    completed.id = "completed-id";
    completed.title = "Quick stretch";
    completed.scheduleType = remindme::ScheduleType::Relative;
    completed.intervalSeconds = 1200;
    completed.completedAt = QDateTime::currentDateTime();
    completed.completionCount = 3;
    sourceStore.completedItems().push_back(completed);

    QString err;
    if (!sourceStore.save(err))
        return false;

    remindme::ReminderStore loadedStore(storagePath);
    if (!loadedStore.load(err))
        return false;

    const Reminder *loadedRepeating = findReminderByTitle(loadedStore, "Daily Standup");
    const Reminder *loadedOneShot = findReminderByTitle(loadedStore, "Book dentist");
    const Reminder *loadedWeekly = findReminderByTitle(loadedStore, "Weekly Sync");
    if (!loadedRepeating || !loadedOneShot || !loadedWeekly)
        return false;

    if (!loadedRepeating->repeating || loadedRepeating->checklistItems.size() != 2)
        return false;
    if (!loadedOneShot->checklistItems.isEmpty())
        return false;
    if (loadedWeekly->repeatWeekdaysMask !=
        (remindme::WeekdayUtils::weekdayBit(2) |
         remindme::WeekdayUtils::weekdayBit(4)))
        return false;

    const auto &completedItems = loadedStore.completedItems();
    if (completedItems.size() != 1)
        return false;

    return completedItems[0].completionCount == 3;
}

bool testStoreMigratesLegacyPath()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString legacyPath = tempDir.filePath("legacy/reminders.json");
    const QString newPath = tempDir.filePath("documents/RemindMe/reminders.json");
    QDir().mkpath(QFileInfo(legacyPath).absolutePath());

    remindme::ReminderStore legacyStore(legacyPath);
    Reminder legacyReminder;
    legacyReminder.id = "legacy-id";
    legacyReminder.title = "Legacy Reminder";
    legacyReminder.repeating = false;
    legacyReminder.scheduleType = remindme::ScheduleType::Relative;
    legacyReminder.intervalSeconds = 900;
    legacyReminder.nextLocal = QDateTime::currentDateTime().addSecs(900);
    legacyStore.items().push_back(legacyReminder);

    QString err;
    if (!legacyStore.save(err))
        return false;
    if (!QFile::exists(legacyPath))
        return false;

    remindme::ReminderStore migratedStore(newPath, legacyPath);
    if (!migratedStore.load(err))
        return false;

    if (!QFile::exists(newPath))
        return false;

    const Reminder *loaded = findReminderByTitle(migratedStore, "Legacy Reminder");
    return loaded != nullptr;
}

bool testStoreSortSoonestFirst()
{
    remindme::ReminderStore store;

    Reminder third;
    third.id = "third";
    third.title = "Third";
    third.nextLocal = QDateTime::currentDateTime().addSecs(300);

    Reminder first;
    first.id = "first";
    first.title = "First";
    first.nextLocal = QDateTime::currentDateTime().addSecs(30);

    Reminder second;
    second.id = "second";
    second.title = "Second";
    second.nextLocal = QDateTime::currentDateTime().addSecs(120);

    store.items().push_back(third);
    store.items().push_back(first);
    store.items().push_back(second);

    store.sortSoonestFirst();
    if (store.items().size() != 3)
        return false;

    return store.items()[0].id == "first" &&
           store.items()[1].id == "second" &&
           store.items()[2].id == "third";
}

bool testStoreLoadSkipsInvalidReminderEntries()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString storagePath = tempDir.filePath("documents/RemindMe/reminders.json");
    QDir().mkpath(QFileInfo(storagePath).absolutePath());

    QFile out(storagePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    const qint64 nowEpoch = QDateTime::currentDateTime().toSecsSinceEpoch();
    const QByteArray json = QString(
                                "{"
                                "\"version\":4,"
                                "\"reminders\":["
                                "{\"id\":\"valid-id\",\"title\":\"Valid Reminder\",\"next_epoch\":%1,\"repeating\":false,\"schedule\":\"relative\",\"interval_seconds\":60,\"time_of_day\":\"\",\"repeat_weekdays_mask\":0},"
                                "{\"id\":\"invalid-id\",\"title\":\"\",\"next_epoch\":0,\"repeating\":false,\"schedule\":\"relative\",\"interval_seconds\":60,\"time_of_day\":\"\",\"repeat_weekdays_mask\":0}"
                                "],"
                                "\"completed\":[]"
                                "}")
                                .arg(nowEpoch)
                                .toUtf8();
    out.write(json);
    out.close();

    remindme::ReminderStore store(storagePath);
    QString err;
    if (!store.load(err))
        return false;

    return store.items().size() == 1 &&
           store.items()[0].id == "valid-id" &&
           store.items()[0].title == "Valid Reminder";
}

bool testStoreLoadMissingFileIsFirstRun()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString storagePath = tempDir.filePath("documents/RemindMe/reminders.json");
    remindme::ReminderStore store(storagePath);
    QString err;
    if (!store.load(err))
        return false;

    return store.items().isEmpty() && store.completedItems().isEmpty();
}

bool testStoreLoadCorruptJsonFails()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString storagePath = tempDir.filePath("documents/RemindMe/reminders.json");
    QDir().mkpath(QFileInfo(storagePath).absolutePath());

    QFile out(storagePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    out.write("{not valid json");
    out.close();

    remindme::ReminderStore store(storagePath);
    QString err;
    return !store.load(err) && !err.isEmpty();
}

bool testStoreLoadCapsCompletedHistory()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString storagePath = tempDir.filePath("documents/RemindMe/reminders.json");
    QDir().mkpath(QFileInfo(storagePath).absolutePath());

    QJsonArray completed;
    const qint64 nowEpoch = QDateTime::currentDateTime().toSecsSinceEpoch();
    for (int i = 0; i < 55; ++i)
    {
        QJsonObject item;
        item["id"] = QString("done-%1").arg(i);
        item["title"] = QString("Done %1").arg(i);
        item["schedule"] = "relative";
        item["interval_seconds"] = 60;
        item["time_of_day"] = "";
        item["repeat_weekdays_mask"] = 0;
        item["completed_epoch"] = static_cast<double>(nowEpoch + i);
        item["completion_count"] = i + 1;
        completed.push_back(item);
    }

    QJsonObject root;
    root["version"] = 4;
    root["reminders"] = QJsonArray();
    root["completed"] = completed;

    QFile out(storagePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    out.close();

    remindme::ReminderStore store(storagePath);
    QString err;
    if (!store.load(err))
        return false;
    if (store.completedItems().size() != 50)
        return false;

    return store.completedItems().first().id == "done-5" &&
           store.completedItems().last().id == "done-54";
}

bool testImportSortsSoonestFirst()
{
    const qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
    const QByteArray raw = QString(
                               "{"
                               "\"format\":\"remindme_share\","
                               "\"version\":1,"
                               "\"reminders\":["
                               "{\"title\":\"Later\",\"next_epoch\":%1,\"repeating\":false,\"schedule\":\"relative\",\"interval_seconds\":60,\"time_of_day\":\"\",\"repeat_weekdays_mask\":0},"
                               "{\"title\":\"Sooner\",\"next_epoch\":%2,\"repeating\":false,\"schedule\":\"relative\",\"interval_seconds\":60,\"time_of_day\":\"\",\"repeat_weekdays_mask\":0}"
                               "]"
                               "}")
                               .arg(now + 600)
                               .arg(now + 60)
                               .toUtf8();
    const QString share = "RM1:" + QString::fromLatin1(
                                       raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    remindme::ReminderStore store;
    int importedCount = 0;
    QString err;
    if (!store.importShareString(share, importedCount, err))
        return false;
    if (importedCount != 2)
        return false;
    if (store.items().size() != 2)
        return false;

    return store.items()[0].title == "Sooner" &&
           store.items()[1].title == "Later";
}

bool testImportPlainJsonAssignsFreshIds()
{
    const qint64 nowEpoch = QDateTime::currentDateTime().toSecsSinceEpoch();

    QJsonObject reminder;
    reminder["id"] = "incoming-id";
    reminder["title"] = "Imported Plain";
    reminder["next_epoch"] = static_cast<double>(nowEpoch + 300);
    reminder["repeating"] = false;
    reminder["schedule"] = "relative";
    reminder["interval_seconds"] = 60;
    reminder["time_of_day"] = "";
    reminder["repeat_weekdays_mask"] = 0;

    QJsonObject root;
    root["format"] = "remindme_share";
    root["version"] = 1;
    root["reminders"] = QJsonArray{reminder};
    const QString payload = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    remindme::ReminderStore store;
    int importedCount = 0;
    QString err;
    if (!store.importShareString(payload, importedCount, err))
        return false;
    if (importedCount != 1 || store.items().size() != 1)
        return false;

    const Reminder &imported = store.items().first();
    return imported.title == "Imported Plain" &&
           !imported.id.isEmpty() &&
           imported.id != "incoming-id";
}

bool testUpdateVersionComparison()
{
    return remindme::UpdateUtils::isRemoteVersionNewer("v1.2.2", "1.2.1") &&
           remindme::UpdateUtils::isRemoteVersionNewer("1.3.0", "1.2.9") &&
           remindme::UpdateUtils::isRemoteVersionNewer(" 2.0.0 ", "1.9.9") &&
           !remindme::UpdateUtils::isRemoteVersionNewer("1.2.1", "1.2.1") &&
           !remindme::UpdateUtils::isRemoteVersionNewer("1.2", "1.1.0") &&
           !remindme::UpdateUtils::isRemoteVersionNewer("v1.2.1", "bad");
}

bool testPickBestReleaseAssetPrefersInstaller()
{
    QJsonArray assets;

    QJsonObject zipAsset;
    zipAsset["name"] = "RemindMe-1.2.2-windows-portable.zip";
    zipAsset["browser_download_url"] = "https://example.com/RemindMe-1.2.2-windows-portable.zip";
    assets.push_back(zipAsset);

    QJsonObject msiAsset;
    msiAsset["name"] = "RemindMe-1.2.2-installer.msi";
    msiAsset["browser_download_url"] = "https://example.com/RemindMe-1.2.2-installer.msi";
    assets.push_back(msiAsset);

    QJsonObject setupAsset;
    setupAsset["name"] = "RemindMe-1.2.2-setup.exe";
    setupAsset["browser_download_url"] = "https://example.com/RemindMe-1.2.2-setup.exe";
    setupAsset["digest"] = "sha256:ABCDEF1234";
    assets.push_back(setupAsset);

    const remindme::UpdateUtils::UpdateAssetInfo best = remindme::UpdateUtils::pickBestReleaseAsset(assets);
    return best.name == "RemindMe-1.2.2-setup.exe" &&
           best.downloadUrl == QUrl("https://example.com/RemindMe-1.2.2-setup.exe") &&
           best.isInstaller &&
           best.sha256Hex == "abcdef1234";
}

bool testPickBestReleaseAssetHandlesMissingFields()
{
    QJsonArray assets;

    assets.push_back(QJsonValue("not-an-object"));

    QJsonObject missingName;
    missingName["name"] = "";
    missingName["browser_download_url"] = "https://example.com/no-name.zip";
    assets.push_back(missingName);

    QJsonObject missingUrl;
    missingUrl["name"] = "RemindMe-1.2.2-setup.exe";
    missingUrl["browser_download_url"] = "";
    assets.push_back(missingUrl);

    const remindme::UpdateUtils::UpdateAssetInfo best = remindme::UpdateUtils::pickBestReleaseAsset(assets);
    return best.name.isEmpty() &&
           best.sha256Hex.isEmpty() &&
           !best.downloadUrl.isValid() &&
           !best.isInstaller;
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
    if (!testParseTimeAmPmBoundaries())
    {
        std::cerr << "testParseTimeAmPmBoundaries failed\n";
        return 1;
    }
    if (!testParseRepeat())
    {
        std::cerr << "testParseRepeat failed\n";
        return 1;
    }
    if (!testParseRepeatWeekday())
    {
        std::cerr << "testParseRepeatWeekday failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayWithEtc())
    {
        std::cerr << "testParseRepeatWeekdayWithEtc failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdaysAlias())
    {
        std::cerr << "testParseRepeatWeekdaysAlias failed\n";
        return 1;
    }
    if (!testParseRepeatWeekendsAlias())
    {
        std::cerr << "testParseRepeatWeekendsAlias failed\n";
        return 1;
    }
    if (!testParseRepeatMultipleWeekdays())
    {
        std::cerr << "testParseRepeatMultipleWeekdays failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayRangeDash())
    {
        std::cerr << "testParseRepeatWeekdayRangeDash failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayRangeThrough())
    {
        std::cerr << "testParseRepeatWeekdayRangeThrough failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayRangeThru())
    {
        std::cerr << "testParseRepeatWeekdayRangeThru failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayFromTo())
    {
        std::cerr << "testParseRepeatWeekdayFromTo failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayRangeWrapAround())
    {
        std::cerr << "testParseRepeatWeekdayRangeWrapAround failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayUnicodeDash())
    {
        std::cerr << "testParseRepeatWeekdayUnicodeDash failed\n";
        return 1;
    }
    if (!testParseRepeatWeekdayPluralTokens())
    {
        std::cerr << "testParseRepeatWeekdayPluralTokens failed\n";
        return 1;
    }
    if (!testRejectWeekdayRepeatOnRelativeReminder())
    {
        std::cerr << "testRejectWeekdayRepeatOnRelativeReminder failed\n";
        return 1;
    }
    if (!testRejectMalformedWeekdayRange())
    {
        std::cerr << "testRejectMalformedWeekdayRange failed\n";
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
    if (!testRejectNonIntegralMathDuration())
    {
        std::cerr << "testRejectNonIntegralMathDuration failed\n";
        return 1;
    }
    if (!testRejectOverflowDuration())
    {
        std::cerr << "testRejectOverflowDuration failed\n";
        return 1;
    }
    if (!testRejectSecondRepeatInterval())
    {
        std::cerr << "testRejectSecondRepeatInterval failed\n";
        return 1;
    }
    if (!testParseDurationExpressionPrecedence())
    {
        std::cerr << "testParseDurationExpressionPrecedence failed\n";
        return 1;
    }
    if (!testWeekdayMaskSpecMixedSeparators())
    {
        std::cerr << "testWeekdayMaskSpecMixedSeparators failed\n";
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
    if (!testStoreSaveLoadRoundTrip())
    {
        std::cerr << "testStoreSaveLoadRoundTrip failed\n";
        return 1;
    }
    if (!testStoreMigratesLegacyPath())
    {
        std::cerr << "testStoreMigratesLegacyPath failed\n";
        return 1;
    }
    if (!testStoreSortSoonestFirst())
    {
        std::cerr << "testStoreSortSoonestFirst failed\n";
        return 1;
    }
    if (!testStoreLoadSkipsInvalidReminderEntries())
    {
        std::cerr << "testStoreLoadSkipsInvalidReminderEntries failed\n";
        return 1;
    }
    if (!testStoreLoadMissingFileIsFirstRun())
    {
        std::cerr << "testStoreLoadMissingFileIsFirstRun failed\n";
        return 1;
    }
    if (!testStoreLoadCorruptJsonFails())
    {
        std::cerr << "testStoreLoadCorruptJsonFails failed\n";
        return 1;
    }
    if (!testStoreLoadCapsCompletedHistory())
    {
        std::cerr << "testStoreLoadCapsCompletedHistory failed\n";
        return 1;
    }
    if (!testImportSortsSoonestFirst())
    {
        std::cerr << "testImportSortsSoonestFirst failed\n";
        return 1;
    }
    if (!testImportPlainJsonAssignsFreshIds())
    {
        std::cerr << "testImportPlainJsonAssignsFreshIds failed\n";
        return 1;
    }
    if (!testUpdateVersionComparison())
    {
        std::cerr << "testUpdateVersionComparison failed\n";
        return 1;
    }
    if (!testPickBestReleaseAssetPrefersInstaller())
    {
        std::cerr << "testPickBestReleaseAssetPrefersInstaller failed\n";
        return 1;
    }
    if (!testPickBestReleaseAssetHandlesMissingFields())
    {
        std::cerr << "testPickBestReleaseAssetHandlesMissingFields failed\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
