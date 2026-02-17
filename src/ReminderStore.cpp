#include "ReminderStore.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>
#include <algorithm>

namespace
{
constexpr int kStorageVersion = 3;
constexpr int kMaxCompletedItems = 50;
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
    const QJsonArray arr = root.value("reminders").toArray();

    for (const auto &v : arr)
    {
        if (!v.isObject())
            continue;
        QJsonObject o = v.toObject();

        Reminder r;
        r.id = o.value("id").toString();
        r.title = o.value("title").toString();

        const qint64 epoch = o.value("next_epoch").toVariant().toLongLong();
        r.nextLocal = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::LocalTime);

        r.repeating = o.value("repeating").toBool(false);

        const QString schedule = o.value("schedule").toString("relative");
        if (schedule == "at_time")
            r.scheduleType = ScheduleType::AtTimeOfDay;
        else
            r.scheduleType = ScheduleType::Relative;

        r.intervalSeconds = o.value("interval_seconds").toInt(0);

        const QString tod = o.value("time_of_day").toString(); // "HH:mm"
        if (!tod.isEmpty())
            r.timeOfDay = QTime::fromString(tod, "HH:mm");

        if (r.id.isEmpty() || r.title.isEmpty() || !r.nextLocal.isValid())
            continue;
        m_items.push_back(r);
    }

    const QJsonArray completedArr = root.value("completed").toArray();
    for (const auto &v : completedArr)
    {
        if (!v.isObject())
            continue;

        const QJsonObject o = v.toObject();
        CompletedReminder completed;
        completed.id = o.value("id").toString();
        completed.title = o.value("title").toString();

        const QString schedule = o.value("schedule").toString("relative");
        if (schedule == "at_time")
            completed.scheduleType = ScheduleType::AtTimeOfDay;
        else
            completed.scheduleType = ScheduleType::Relative;

        completed.intervalSeconds = o.value("interval_seconds").toInt(0);

        const QString timeOfDay = o.value("time_of_day").toString();
        if (!timeOfDay.isEmpty())
            completed.timeOfDay = QTime::fromString(timeOfDay, "HH:mm");

        const qint64 completedEpoch = o.value("completed_epoch").toVariant().toLongLong();
        completed.completedAt = QDateTime::fromSecsSinceEpoch(completedEpoch, QTimeZone::LocalTime);
        completed.completionCount = std::max(1, o.value("completion_count").toInt(1));

        if (completed.id.isEmpty() || completed.title.isEmpty() || !completed.completedAt.isValid())
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

    QJsonArray arr;
    for (const auto &r : m_items)
    {
        QJsonObject o;
        o["id"] = r.id;
        o["title"] = r.title;
        o["next_epoch"] = static_cast<double>(r.nextLocal.toSecsSinceEpoch());
        o["repeating"] = r.repeating;
        o["schedule"] = (r.scheduleType == ScheduleType::AtTimeOfDay) ? "at_time" : "relative";
        o["interval_seconds"] = r.intervalSeconds;
        o["time_of_day"] = r.timeOfDay.isValid() ? r.timeOfDay.toString("HH:mm") : "";
        arr.push_back(o);
    }

    QJsonArray completedArr;
    for (const auto &completed : m_completedItems)
    {
        QJsonObject o;
        o["id"] = completed.id;
        o["title"] = completed.title;
        o["schedule"] = (completed.scheduleType == ScheduleType::AtTimeOfDay) ? "at_time" : "relative";
        o["interval_seconds"] = completed.intervalSeconds;
        o["time_of_day"] = completed.timeOfDay.isValid() ? completed.timeOfDay.toString("HH:mm") : "";
        o["completed_epoch"] = static_cast<double>(completed.completedAt.toSecsSinceEpoch());
        o["completion_count"] = completed.completionCount;
        completedArr.push_back(o);
    }

    QJsonObject root;
    root["version"] = kStorageVersion;
    root["reminders"] = arr;
    root["completed"] = completedArr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        outError = "Failed to open reminders file for writing.";
        return false;
    }

    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void ReminderStore::sortSoonestFirst()
{
    std::sort(m_items.begin(), m_items.end(), [](const Reminder &a, const Reminder &b)
              { return a.nextLocal < b.nextLocal; });
}
