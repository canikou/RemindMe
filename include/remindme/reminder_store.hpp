#pragma once
#include "remindme/reminder.hpp"
#include <QVector>
#include <QString>

namespace remindme
{

class ReminderStore
{
public:
    explicit ReminderStore(QString storagePathOverride = {},
                           QString legacyStoragePathOverride = {});

    bool load(QString &outError);
    bool save(QString &outError) const;
    QString exportShareString(QString &outError) const;
    bool importShareString(const QString &shareString, int &outImportedCount, QString &outError);

    QVector<Reminder> &items() { return m_items; }
    const QVector<Reminder> &items() const { return m_items; }
    QVector<CompletedReminder> &completedItems() { return m_completedItems; }
    const QVector<CompletedReminder> &completedItems() const { return m_completedItems; }

    void sortSoonestFirst();

    QString storagePath() const;
    QString legacyStoragePath() const;

private:
    QString m_storagePathOverride;
    QString m_legacyStoragePathOverride;
    QVector<Reminder> m_items;
    QVector<CompletedReminder> m_completedItems;
};

}
