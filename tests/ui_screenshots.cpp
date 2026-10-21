// SPDX-License-Identifier: MIT
//
// CI-only helper: renders the main window, overlay and popup with sample data
// in light and dark themes and saves PNGs, so UI changes can be reviewed
// without a local Qt install. It overwrites the real reminders.json, so it
// refuses to run outside CI unless REMINDME_SCREENSHOTS_OK=1 is set.

#include "remindme/main_window.hpp"
#include "remindme/reminder_popup.hpp"
#include "remindme/reminder_store.hpp"

#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QUuid>

#include <iostream>

namespace
{
using remindme::CompletedReminder;
using remindme::Reminder;
using remindme::ScheduleType;

Reminder makeReminder(const QString &title, qint64 secondsFromNow, bool repeating, int intervalSeconds, int weekdayMask = 0)
{
    Reminder r;
    r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.title = title;
    r.nextLocal = QDateTime::currentDateTime().addSecs(secondsFromNow);
    r.repeating = repeating;
    r.intervalSeconds = intervalSeconds;
    r.scheduleType = (weekdayMask != 0 || intervalSeconds == 0) ? ScheduleType::AtTimeOfDay : ScheduleType::Relative;
    r.timeOfDay = r.nextLocal.time();
    r.repeatWeekdaysMask = weekdayMask;
    return r;
}

bool writeSampleData(bool withReminders)
{
    remindme::ReminderStore store;
    QString err;
    store.load(err);
    store.items().clear();
    store.completedItems().clear();

    if (withReminders)
    {
        store.items().push_back(makeReminder("Stand up and stretch", 18 * 60 + 42, true, 2 * 3600));
        Reminder meds = makeReminder("Take vitamins", 5 * 3600 + 48 * 60, true, 0, 0b0011111);
        meds.checklistItems = {{"Vitamin D", true}, {"Fish oil", false}, {"Refill water bottle", false}};
        store.items().push_back(meds);
        store.items().push_back(makeReminder("Call the dentist about Thursday's appointment", 2 * 3600 + 4 * 60, false, 0));
        store.items().push_back(makeReminder("Reinforce", 3 * 86400 + 20 * 3600, false, 0));
        store.items().push_back(makeReminder("Water the plants", 26 * 3600, false, 26 * 3600));

        CompletedReminder done;
        done.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        done.title = "Drink water";
        done.scheduleType = ScheduleType::Relative;
        done.intervalSeconds = 45 * 60;
        done.completedAt = QDateTime::currentDateTime().addSecs(-20 * 60);
        done.completionCount = 3;
        store.completedItems().push_back(done);

        CompletedReminder done2 = done;
        done2.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        done2.title = "Check the oven";
        done2.scheduleType = ScheduleType::AtTimeOfDay;
        done2.timeOfDay = QTime(8, 30);
        done2.completedAt = QDateTime::currentDateTime().addSecs(-3 * 3600);
        done2.completionCount = 1;
        store.completedItems().push_back(done2);
    }

    if (!store.save(err))
    {
        std::cerr << "save failed: " << err.toStdString() << "\n";
        return false;
    }
    return true;
}

void settle(int ms = 400)
{
    for (int i = 0; i < ms / 20; ++i)
    {
        QApplication::processEvents();
        QThread::msleep(20);
    }
}

bool saveGrab(QWidget *widget, const QString &path)
{
    const bool ok = widget->grab().save(path);
    std::cout << (ok ? "wrote " : "FAILED ") << path.toStdString() << "\n";
    return ok;
}
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("CI") && qEnvironmentVariable("REMINDME_SCREENSHOTS_OK") != "1")
    {
        std::cerr << "Refusing to run: this overwrites reminders.json. Set REMINDME_SCREENSHOTS_OK=1 to force.\n";
        return 2;
    }

    QApplication app(argc, argv);
    app.setApplicationName("RemindMe");
    app.setQuitOnLastWindowClosed(false);

    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("ui-screenshots");
    QDir().mkpath(outDir);
    bool ok = true;

    for (const char *theme : {"light", "dark"})
    {
        qputenv("REMINDME_THEME", theme);
        const QString suffix = QString::fromLatin1(theme);

        if (!writeSampleData(true))
            return 1;
        {
            remindme::MainWindow window;
            window.resize(1020, 700);
            window.show();
            settle();
            ok &= saveGrab(&window, QDir(outDir).filePath("main-" + suffix + ".png"));

            if (auto *input = window.findChild<QLineEdit *>())
            {
                input->setText("Water plants in 45m every day");
                settle(100);
                ok &= saveGrab(&window, QDir(outDir).filePath("main-typing-" + suffix + ".png"));
                input->setText("Water plants tomorrow");
                QMetaObject::invokeMethod(input, "returnPressed");
                settle(100);
                ok &= saveGrab(&window, QDir(outDir).filePath("main-error-" + suffix + ".png"));
                input->clear();
            }

            for (QPushButton *button : window.findChildren<QPushButton *>())
            {
                if (button->text() == "Overlay")
                    button->click();
            }
            settle();
            for (QWidget *top : QApplication::topLevelWidgets())
            {
                if (top->objectName() == "overlayWindow" && top->isVisible())
                {
                    ok &= saveGrab(top, QDir(outDir).filePath("overlay-" + suffix + ".png"));
                    top->hide(); // the overlay has no parent, so it outlives this window
                }
            }

            remindme::ReminderPopup popup("sample", "Stand up and stretch", QDateTime::currentDateTime().addSecs(-95));
            popup.show();
            settle();
            ok &= saveGrab(&popup, QDir(outDir).filePath("popup-" + suffix + ".png"));
            popup.hide();
        }

        if (!writeSampleData(false))
            return 1;
        {
            remindme::MainWindow window;
            window.resize(1020, 700);
            window.show();
            settle();
            ok &= saveGrab(&window, QDir(outDir).filePath("main-empty-" + suffix + ".png"));
        }
    }

    return ok ? 0 : 1;
}
