#include "remindme/reminder_store.hpp"

#include <QByteArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>

namespace remindme
{

namespace
{
constexpr int kStorageVersion = 4;
constexpr int kShareVersion = 1;
constexpr int kMaxCompletedItems = 50;
constexpr const char *kSharePrefix = "RM1:";

QJsonArray checklistItemsToJson(const Reminder &reminder)
{
    QJsonArray checklist;
    for (const Reminder::ChecklistItem &item : reminder.checklistItems)
    {
        const QString text = item.text.trimmed();
        if (text.isEmpty())
            continue;

        QJsonObject checklistObject;
        checklistObject["text"] = text;
        checklistObject["checked"] = item.checked;
        checklist.push_back(checklistObject);
    }
    return checklist;
}

void checklistItemsFromJson(const QJsonArray &checklistArray, Reminder &reminder)
{
    reminder.checklistItems.clear();
    for (const QJsonValue &value : checklistArray)
    {
        if (!value.isObject())
            continue;

        const QJsonObject checklistObject = value.toObject();
        const QString text = checklistObject.value("text").toString().trimmed();
        if (text.isEmpty())
            continue;

        Reminder::ChecklistItem item;
        item.text = text;
        item.checked = checklistObject.value("checked").toBool(false);
        reminder.checklistItems.push_back(item);
    }
}

QJsonObject reminderToJson(const Reminder &reminder)
{
    QJsonObject object;
    object["id"] = reminder.id;
    object["title"] = reminder.title;
    object["next_epoch"] = static_cast<double>(reminder.nextLocal.toSecsSinceEpoch());
    object["repeating"] = reminder.repeating;
    object["schedule"] = (reminder.scheduleType == ScheduleType::AtTimeOfDay) ? "at_time" : "relative";
    object["interval_seconds"] = reminder.intervalSeconds;
    object["time_of_day"] = reminder.timeOfDay.isValid() ? reminder.timeOfDay.toString("HH:mm") : "";
    object["checklist_items"] = checklistItemsToJson(reminder);
    return object;
}

bool reminderFromJson(const QJsonObject &object, Reminder &outReminder, bool requireId)
{
    Reminder reminder;
    reminder.id = object.value("id").toString();
    reminder.title = object.value("title").toString();

    const qint64 epoch = object.value("next_epoch").toVariant().toLongLong();
    reminder.nextLocal = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::LocalTime);

    reminder.repeating = object.value("repeating").toBool(false);

    const QString schedule = object.value("schedule").toString("relative");
    if (schedule == "at_time")
        reminder.scheduleType = ScheduleType::AtTimeOfDay;
    else
        reminder.scheduleType = ScheduleType::Relative;

    reminder.intervalSeconds = object.value("interval_seconds").toInt(0);

    const QString timeOfDay = object.value("time_of_day").toString();
    if (!timeOfDay.isEmpty())
        reminder.timeOfDay = QTime::fromString(timeOfDay, "HH:mm");

    checklistItemsFromJson(object.value("checklist_items").toArray(), reminder);
    reminder.enforceChecklistConstraints();

    if ((requireId && reminder.id.isEmpty()) || reminder.title.isEmpty() || !reminder.nextLocal.isValid())
        return false;

    outReminder = reminder;
    return true;
}

QJsonObject completedReminderToJson(const CompletedReminder &completed)
{
    QJsonObject object;
    object["id"] = completed.id;
    object["title"] = completed.title;
    object["schedule"] = (completed.scheduleType == ScheduleType::AtTimeOfDay) ? "at_time" : "relative";
    object["interval_seconds"] = completed.intervalSeconds;
    object["time_of_day"] = completed.timeOfDay.isValid() ? completed.timeOfDay.toString("HH:mm") : "";
    object["completed_epoch"] = static_cast<double>(completed.completedAt.toSecsSinceEpoch());
    object["completion_count"] = completed.completionCount;
    return object;
}

bool completedReminderFromJson(const QJsonObject &object, CompletedReminder &outCompleted)
{
    CompletedReminder completed;
    completed.id = object.value("id").toString();
    completed.title = object.value("title").toString();

    const QString schedule = object.value("schedule").toString("relative");
    if (schedule == "at_time")
        completed.scheduleType = ScheduleType::AtTimeOfDay;
    else
        completed.scheduleType = ScheduleType::Relative;

    completed.intervalSeconds = object.value("interval_seconds").toInt(0);

    const QString timeOfDay = object.value("time_of_day").toString();
    if (!timeOfDay.isEmpty())
        completed.timeOfDay = QTime::fromString(timeOfDay, "HH:mm");

    const qint64 completedEpoch = object.value("completed_epoch").toVariant().toLongLong();
    completed.completedAt = QDateTime::fromSecsSinceEpoch(completedEpoch, QTimeZone::LocalTime);
    completed.completionCount = std::max(1, object.value("completion_count").toInt(1));

    if (completed.id.isEmpty() || completed.title.isEmpty() || !completed.completedAt.isValid())
        return false;

    outCompleted = completed;
    return true;
}

bool decodeSharePayload(const QString &shareString, QJsonObject &outRoot, QString &outError)
{
    const QString trimmed = shareString.trimmed();
    if (trimmed.isEmpty())
    {
        outError = "Import string is empty.";
        return false;
    }

    QByteArray rawJson;
    const QString sharePrefix = QString::fromLatin1(kSharePrefix);
    if (trimmed.startsWith(sharePrefix))
    {
        const QString encoded = trimmed.mid(sharePrefix.size());
        rawJson = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::Base64UrlEncoding);
        if (rawJson.isEmpty())
        {
            outError = "Import string is not valid RemindMe share data.";
            return false;
        }
    }
    else
    {
        rawJson = trimmed.toUtf8();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(rawJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        outError = "Import string is not valid JSON data.";
        return false;
    }

    outRoot = document.object();
    return true;
}
}

QString ReminderStore::storagePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath("reminders.json");
}

bool ReminderStore::load(QString &outError)
{
    m_items.clear();
    m_completedItems.clear();

    QFile f(storagePath());
    if (!f.exists())
        return true; // first run is fine

    if (!f.open(QIODevice::ReadOnly))
    {
        outError = "Failed to open reminders file for reading.";
        return false;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        outError = "Reminders file is corrupted (not a JSON object).";
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray reminderArray = root.value("reminders").toArray();
    for (const QJsonValue &value : reminderArray)
    {
        if (!value.isObject())
            continue;

        Reminder reminder;
        if (!reminderFromJson(value.toObject(), reminder, true))
            continue;

        m_items.push_back(reminder);
    }

    const QJsonArray completedArray = root.value("completed").toArray();
    for (const QJsonValue &value : completedArray)
    {
        if (!value.isObject())
            continue;

        CompletedReminder completed;
        if (!completedReminderFromJson(value.toObject(), completed))
            continue;

        m_completedItems.push_back(completed);
    }

    while (m_completedItems.size() > kMaxCompletedItems)
        m_completedItems.removeFirst();

    sortSoonestFirst();
    return true;
}

bool ReminderStore::save(QString &outError) const
{
    const QString path = storagePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray reminderArray;
    for (const Reminder &reminder : m_items)
    {
        reminderArray.push_back(reminderToJson(reminder));
    }

    QJsonArray completedArray;
    for (const CompletedReminder &completed : m_completedItems)
    {
        completedArray.push_back(completedReminderToJson(completed));
    }

    QJsonObject root;
    root["version"] = kStorageVersion;
    root["reminders"] = reminderArray;
    root["completed"] = completedArray;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        outError = "Failed to open reminders file for writing.";
        return false;
    }

    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QString ReminderStore::exportShareString(QString &outError) const
{
    outError.clear();

    QJsonArray reminderArray;
    for (const Reminder &reminder : m_items)
    {
        reminderArray.push_back(reminderToJson(reminder));
    }

    QJsonObject root;
    root["format"] = "remindme_share";
    root["version"] = kShareVersion;
    root["reminders"] = reminderArray;

    const QByteArray rawJson = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const QByteArray encoded = rawJson.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    if (encoded.isEmpty())
    {
        outError = "Failed to build export string.";
        return {};
    }

    return QString::fromLatin1(kSharePrefix) + QString::fromLatin1(encoded);
}

bool ReminderStore::importShareString(const QString &shareString, int &outImportedCount, QString &outError)
{
    outImportedCount = 0;
    outError.clear();

    QJsonObject root;
    if (!decodeSharePayload(shareString, root, outError))
        return false;

    const QJsonArray reminderArray = root.value("reminders").toArray();
    if (reminderArray.isEmpty())
    {
        outError = "Import data does not contain reminders.";
        return false;
    }

    for (const QJsonValue &value : reminderArray)
    {
        if (!value.isObject())
            continue;

        Reminder reminder;
        if (!reminderFromJson(value.toObject(), reminder, false))
            continue;

        reminder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        reminder.enforceChecklistConstraints();
        m_items.push_back(reminder);
        ++outImportedCount;
    }

    if (outImportedCount == 0)
    {
        outError = "Import string had no valid reminders.";
        return false;
    }

    sortSoonestFirst();
    return true;
}

void ReminderStore::sortSoonestFirst()
{
    std::sort(m_items.begin(), m_items.end(), [](const Reminder &a, const Reminder &b)
              { return a.nextLocal < b.nextLocal; });
}

}
