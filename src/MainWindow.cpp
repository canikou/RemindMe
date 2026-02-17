#include "MainWindow.h"

#include "AppInfo.h"
#include "Parser.h"
#include "ReminderPopup.h"
#include "TimeFormat.h"
#include "WinFocus.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStringConverter>
#include <QTime>
#include <QTimeEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>
#include <QUuid>

#include <algorithm>
#include <array>

namespace
{
constexpr int kTickIntervalMs = 1000;
constexpr int kDefaultReminderStepSeconds = 60;
constexpr int kSnoozeSeconds = 5 * 60;
constexpr int kMinimumRowHeight = 102;
constexpr int kShortRepeatWarningSeconds = 30;
constexpr int kCompletedPreviewCount = 2;
constexpr int kCompletedPreviewRowHeight = 56;
constexpr int kCompletedPreviewListPadding = 8;
constexpr int kMaxCompletedItems = 50;
constexpr const char *kGreetingFileName = "greetings.txt";

enum class GreetingPeriod
{
    Morning = 0,
    Afternoon = 1,
    Evening = 2
};

GreetingPeriod greetingPeriodForHour(int hour)
{
    if (hour >= 5 && hour < 12)
        return GreetingPeriod::Morning;
    if (hour >= 12 && hour < 18)
        return GreetingPeriod::Afternoon;
    return GreetingPeriod::Evening;
}

const QStringList &greetingPool(GreetingPeriod period)
{
    static const QStringList defaultMorningGreetings = {
        "Good morning, have we had coffee yet? ☕",
        "Good morning! Tiny steps still count 🌤️",
        "Morning! Let's keep it easy and steady 🌱",
        "Good morning! You've got this 💪",
        "Morning reset: breathe, then begin 🌬️",
        "Morning check-in: water first 💧",
        "Morning focus: one thing at a time 🎯",
        "Morning mood: calm progress is still progress 🍃",
        "Morning plan: start small, win early ✨",
        "Morning reminder: be kind to yourself today 🌅"
    };

    static const QStringList defaultAfternoonGreetings = {
        "Good afternoon! Hydration check 💧",
        "Afternoon mode: progress over perfection ✨",
        "Good afternoon! One reminder at a time 📌",
        "Afternoon vibes: keep it simple 🌼",
        "Afternoon checkpoint: how is your energy? 🔋",
        "Afternoon reset: quick stretch break? 🤸",
        "Afternoon focus: pick the next tiny step 🎯",
        "Afternoon momentum: progress over perfection 🚀",
        "Afternoon reminder: do not forget to blink 😄",
        "Afternoon vibes: steady pace, steady results 🚶"
    };

    static const QStringList defaultEveningGreetings = {
        "Good evening! Time to wrap up gently 🌙",
        "Evening check-in: what matters next? 📝",
        "Good evening! You made it through today 🌃",
        "Night mode, calm pace 😌",
        "Good evening! Rest counts too 🛌",
        "Good evening! You did enough for today ✅",
        "Evening reset: light tasks only 🕯️",
        "Evening focus: finish strong, not fast 🧹",
        "Evening reminder: let tomorrow handle tomorrow 🌙",
        "Evening mood: quiet progress still counts 🌌"
    };

    static const auto pickPath = []() -> QString
    {
        const QStringList candidates = {
            QDir::current().filePath(kGreetingFileName),
            QCoreApplication::applicationDirPath() + "/" + kGreetingFileName,
            QCoreApplication::applicationDirPath() + "/../" + kGreetingFileName,
        };

        for (const QString &path : candidates)
        {
            if (QFile::exists(path))
                return path;
        }
        return QCoreApplication::applicationDirPath() + "/" + kGreetingFileName;
    };

    static const auto writeGreetingFile = [](const QString &path, const std::array<QStringList, 3> &pools)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());

        QFile outFile(path);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return;

        QTextStream out(&outFile);
        out.setEncoding(QStringConverter::Utf8);
        out << "# Editable greeting message bank for RemindMe.\n";
        out << "# Blank lines and lines starting with # are ignored.\n\n";

        out << "[morning]\n";
        for (const QString &line : pools[static_cast<int>(GreetingPeriod::Morning)])
            out << line << '\n';
        out << '\n';

        out << "[afternoon]\n";
        for (const QString &line : pools[static_cast<int>(GreetingPeriod::Afternoon)])
            out << line << '\n';
        out << '\n';

        out << "[evening]\n";
        for (const QString &line : pools[static_cast<int>(GreetingPeriod::Evening)])
            out << line << '\n';
    };

    static const auto loadGreetings = [&]() -> std::array<QStringList, 3>
    {
        std::array<QStringList, 3> result = {
            defaultMorningGreetings,
            defaultAfternoonGreetings,
            defaultEveningGreetings,
        };

        const QString path = pickPath();
        if (!QFile::exists(path))
            writeGreetingFile(path, result);

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return result;

        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        int section = -1;
        std::array<QStringList, 3> loaded;

        while (!in.atEnd())
        {
            const QString raw = in.readLine();
            const QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;

            if (line == "[morning]")
            {
                section = static_cast<int>(GreetingPeriod::Morning);
                continue;
            }
            if (line == "[afternoon]")
            {
                section = static_cast<int>(GreetingPeriod::Afternoon);
                continue;
            }
            if (line == "[evening]")
            {
                section = static_cast<int>(GreetingPeriod::Evening);
                continue;
            }

            if (section >= 0 && section < static_cast<int>(loaded.size()))
                loaded[section].push_back(line);
        }

        for (int i = 0; i < static_cast<int>(loaded.size()); ++i)
        {
            if (!loaded[i].isEmpty())
                result[i] = loaded[i];
        }

        return result;
    };

    static const std::array<QStringList, 3> loadedGreetings = loadGreetings();

    switch (period)
    {
    case GreetingPeriod::Morning:
        return loadedGreetings[static_cast<int>(GreetingPeriod::Morning)];
    case GreetingPeriod::Afternoon:
        return loadedGreetings[static_cast<int>(GreetingPeriod::Afternoon)];
    case GreetingPeriod::Evening:
    default:
        return loadedGreetings[static_cast<int>(GreetingPeriod::Evening)];
    }
}

QDateTime nextAtTimeLocal(const QTime &timeOfDay)
{
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime next(QDate::currentDate(), timeOfDay);
    if (next <= now)
        next = next.addDays(1);
    return next;
}

QString repeatIndicator()
{
    return QString::fromUcs4(U"\U0001F501");
}

QString durationToInputText(int seconds)
{
    int remaining = qMax(1, seconds);
    QStringList parts;

    const int days = remaining / 86400;
    remaining %= 86400;
    const int hours = remaining / 3600;
    remaining %= 3600;
    const int minutes = remaining / 60;
    const int secs = remaining % 60;

    if (days > 0)
        parts.push_back(QString("%1d").arg(days));
    if (hours > 0)
        parts.push_back(QString("%1h").arg(hours));
    if (minutes > 0)
        parts.push_back(QString("%1m").arg(minutes));
    if (secs > 0 || parts.isEmpty())
        parts.push_back(QString("%1s").arg(secs));

    return parts.join(" ");
}

QString repeatingInfoText(const Reminder &reminder)
{
    if (!reminder.repeating)
        return "(None)";

    if (reminder.scheduleType == ScheduleType::Relative)
    {
        const int interval = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : kDefaultReminderStepSeconds;
        return QString("Every %1").arg(TimeFormat::formatIntervalText(interval));
    }

    return QString("At %1 daily").arg(reminder.timeOfDay.toString("h:mm AP"));
}

QString completedPatternText(const CompletedReminder &completed)
{
    if (completed.scheduleType == ScheduleType::Relative)
    {
        const int interval = (completed.intervalSeconds > 0) ? completed.intervalSeconds : kDefaultReminderStepSeconds;
        return QString("Pattern: in %1").arg(TimeFormat::formatIntervalText(interval));
    }

    return QString("Pattern: at %1").arg(completed.timeOfDay.toString("h:mm AP"));
}

QString completedTitleText(const CompletedReminder &completed)
{
    if (completed.completionCount <= 1)
        return completed.title;

    return QString("%1  (x%2)").arg(completed.title).arg(completed.completionCount);
}

bool isSameCompletedPattern(const CompletedReminder &completed, const Reminder &reminder)
{
    if (completed.title != reminder.title)
        return false;

    if (completed.scheduleType != reminder.scheduleType)
        return false;

    if (completed.scheduleType == ScheduleType::Relative)
    {
        const int completedInterval = (completed.intervalSeconds > 0) ? completed.intervalSeconds : kDefaultReminderStepSeconds;
        const int reminderInterval = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : kDefaultReminderStepSeconds;
        return completedInterval == reminderInterval;
    }

    return completed.timeOfDay == reminder.timeOfDay;
}

void rescheduleAfterAcknowledge(Reminder &reminder, const QDateTime &now)
{
    if (reminder.scheduleType == ScheduleType::Relative)
    {
        const int step = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : kDefaultReminderStepSeconds;
        QDateTime next = reminder.nextLocal;
        if (!next.isValid())
            next = now;

        while (next <= now)
            next = next.addSecs(step);

        reminder.nextLocal = next;
        return;
    }

    reminder.nextLocal = nextAtTimeLocal(reminder.timeOfDay);
}

struct EditReminderValues
{
    QString title;
    ScheduleType scheduleType = ScheduleType::Relative;
    QString relativeDelay;
    QTime timeOfDay;
    bool repeating = false;
    QString repeatInterval;
};

bool showEditReminderDialog(QWidget *parent, const Reminder &reminder, EditReminderValues &out)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Edit Reminder");
    dialog.setModal(true);

    auto *root = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    root->addLayout(form);

    auto *titleEdit = new QLineEdit(reminder.title);
    form->addRow("Title:", titleEdit);

    auto *typeCombo = new QComboBox();
    typeCombo->addItem("In (relative)", static_cast<int>(ScheduleType::Relative));
    typeCombo->addItem("At (time of day)", static_cast<int>(ScheduleType::AtTimeOfDay));
    typeCombo->setCurrentIndex((reminder.scheduleType == ScheduleType::Relative) ? 0 : 1);
    form->addRow("Type:", typeCombo);

    auto *relativeLabel = new QLabel("Next in:");
    auto *relativeEdit = new QLineEdit();
    int defaultRelativeSeconds = qMax(1, QDateTime::currentDateTime().secsTo(reminder.nextLocal));
    if (defaultRelativeSeconds <= 0)
        defaultRelativeSeconds = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : kDefaultReminderStepSeconds;
    relativeEdit->setText(durationToInputText(defaultRelativeSeconds));
    relativeEdit->setPlaceholderText("e.g. 45m, 2h 30m");
    form->addRow(relativeLabel, relativeEdit);

    auto *timeLabel = new QLabel("At:");
    auto *timeEdit = new QTimeEdit();
    timeEdit->setDisplayFormat("h:mm AP");
    timeEdit->setTime(reminder.timeOfDay.isValid() ? reminder.timeOfDay : reminder.nextLocal.time());
    form->addRow(timeLabel, timeEdit);

    auto *repeatingCheck = new QCheckBox("Repeating");
    repeatingCheck->setChecked(reminder.repeating);
    form->addRow("", repeatingCheck);

    auto *repeatLabel = new QLabel("Repeat every:");
    auto *repeatEdit = new QLineEdit();
    const int defaultRepeat = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : defaultRelativeSeconds;
    repeatEdit->setText(durationToInputText(defaultRepeat));
    repeatEdit->setPlaceholderText("e.g. 30m, 2h, 1d");
    form->addRow(repeatLabel, repeatEdit);

    auto updateVisibility = [&]()
    {
        const bool isRelative = (typeCombo->currentIndex() == 0);
        const bool showRepeat = isRelative && repeatingCheck->isChecked();

        relativeLabel->setVisible(isRelative);
        relativeEdit->setVisible(isRelative);

        timeLabel->setVisible(!isRelative);
        timeEdit->setVisible(!isRelative);

        repeatLabel->setVisible(showRepeat);
        repeatEdit->setVisible(showRepeat);
    };

    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, &dialog, updateVisibility);
    QObject::connect(repeatingCheck, &QCheckBox::toggled, &dialog, updateVisibility);
    updateVisibility();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    out.title = titleEdit->text().trimmed();
    out.scheduleType = (typeCombo->currentIndex() == 0) ? ScheduleType::Relative : ScheduleType::AtTimeOfDay;
    out.relativeDelay = relativeEdit->text().trimmed();
    out.timeOfDay = timeEdit->time();
    out.repeating = repeatingCheck->isChecked();
    out.repeatInterval = repeatEdit->text().trimmed();
    return true;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QString("%1 v%2").arg(AppInfo::kAppName, AppInfo::kAppVersion));
    resize(1020, 700);

    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    setStyleSheet(R"(
        QMainWindow { background: #1f1f1f; color: #eaeaea; }
        QLabel { color: #eaeaea; }
        QLineEdit {
            background: #2b2b2b; color: #eaeaea;
            border: 1px solid #3c3c3c; padding: 8px;
        }
        QPushButton {
            background: #3a3a3a; color: #eaeaea;
            border: 1px solid #555; padding: 8px 12px;
        }
        QPushButton:hover { background: #444; }
        QCheckBox { color: #eaeaea; }
    )");

    auto *topRow = new QHBoxLayout();

    titleLabel = new QLabel();
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: 700;");

    viewAllCompletedBtn = new QPushButton("Reminder History");
    viewAllCompletedBtn->setStyleSheet("font-size: 12px; padding: 6px 10px;");
    connect(viewAllCompletedBtn, &QPushButton::clicked, this, &MainWindow::showAllCompletedDialog);

    topRow->addWidget(titleLabel, 1);
    topRow->addWidget(viewAllCompletedBtn, 0, Qt::AlignRight);
    v->addLayout(topRow);

    nowLabel = new QLabel();
    nowLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    nowLabel->setStyleSheet("font-size: 42px; font-weight: 750;");
    v->addWidget(nowLabel);

    stacked = new QStackedWidget();

    emptyLabel = new QLabel("No Reminders Yet!");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("font-size: 22px; opacity: 0.85;");

    list = new QListWidget();
    list->setStyleSheet(R"(
        QListWidget { background: #171717; border: 1px solid #2a2a2a; }
        QListWidget::item { border-bottom: 1px solid #242424; }
    )");
    list->setSpacing(0);
    list->setUniformItemSizes(false);

    stacked->addWidget(emptyLabel);
    stacked->addWidget(list);
    v->addWidget(stacked, 1);

    completedHeaderLabel = new QLabel("Completed");
    completedHeaderLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #cccccc;");
    v->addWidget(completedHeaderLabel);

    completedList = new QListWidget();
    completedList->setStyleSheet(R"(
        QListWidget { background: #151515; border: 1px solid #2a2a2a; }
        QListWidget::item { border-bottom: 1px solid #242424; }
    )");
    completedList->setSpacing(0);
    completedList->setUniformItemSizes(false);
    completedList->setFixedHeight((kCompletedPreviewCount * kCompletedPreviewRowHeight) + kCompletedPreviewListPadding);
    v->addWidget(completedList);

    auto *h = new QHBoxLayout();
    input = new QLineEdit();
    input->setPlaceholderText(R"(e.g. "Drink water in 45m" or "Stand up at 7:00AM every day")");
    h->addWidget(input, 1);
    v->addLayout(h);

    setCentralWidget(central);

    connect(input, &QLineEdit::returnPressed, this, &MainWindow::onAddClicked);

    QString err;
    if (!store.load(err))
        QMessageBox::warning(this, "Load error", err);

    updateGreetingMessage();
    nowLabel->setText(TimeFormat::formatClockTime(QDateTime::currentDateTime()));
    triggerDueReminders();
    refreshUI();

    tickTimer = new QTimer(this);
    tickTimer->setInterval(kTickIntervalMs);
    connect(tickTimer, &QTimer::timeout, this, &MainWindow::onTick);
    tickTimer->start();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    saveStoreBestEffort();
    QMainWindow::closeEvent(e);
}

void MainWindow::updateGreetingMessage()
{
    const GreetingPeriod period = greetingPeriodForHour(QTime::currentTime().hour());
    const int periodValue = static_cast<int>(period);

    if (periodValue == currentGreetingPeriod && !titleLabel->text().isEmpty())
        return;

    currentGreetingPeriod = periodValue;

    const QStringList &pool = greetingPool(period);
    if (pool.isEmpty())
        return;

    const int pick = QRandomGenerator::global()->bounded(pool.size());
    titleLabel->setText(pool.at(pick));
}

void MainWindow::onTick()
{
    updateGreetingMessage();
    nowLabel->setText(TimeFormat::formatClockTime(QDateTime::currentDateTime()));
    triggerDueReminders();
    updateCountdownLabels();
}

void MainWindow::onAddClicked()
{
    const QString text = input->text().trimmed();
    if (text.isEmpty())
        return;

    const ParseResult parsed = Parser::parseInput(text);
    if (!parsed.ok)
    {
        QMessageBox::warning(this, "Parse error", parsed.error);
        return;
    }

    const bool shouldRepeat = parsed.hasRepeatDirective;

    Reminder reminder;
    reminder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    reminder.title = parsed.title;
    reminder.repeating = shouldRepeat;

    if (parsed.isRelative)
    {
        reminder.scheduleType = ScheduleType::Relative;
        reminder.nextLocal = QDateTime::currentDateTime().addSecs(parsed.durationSeconds);
        reminder.intervalSeconds = shouldRepeat
                                       ? (parsed.hasRepeatDirective ? parsed.repeatIntervalSeconds : parsed.durationSeconds)
                                       : parsed.durationSeconds;
    }
    else
    {
        reminder.timeOfDay = parsed.timeOfDay;
        reminder.nextLocal = nextAtTimeLocal(parsed.timeOfDay);

        if (shouldRepeat && parsed.hasRepeatDirective && parsed.repeatIntervalSeconds != 86400)
        {
            reminder.scheduleType = ScheduleType::Relative;
            reminder.intervalSeconds = parsed.repeatIntervalSeconds;
        }
        else
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.intervalSeconds = 0;
        }
    }

    if (reminder.repeating &&
        reminder.scheduleType == ScheduleType::Relative &&
        reminder.intervalSeconds > 0 &&
        reminder.intervalSeconds < kShortRepeatWarningSeconds)
    {
        if (!confirmShortRepeatInterval(reminder.intervalSeconds, "adding"))
            return;
    }

    store.items().push_back(reminder);
    commitReminderChanges();
    input->clear();
}

void MainWindow::deleteReminderById(const QString &id)
{
    auto &items = store.items();
    for (int i = 0; i < items.size(); ++i)
    {
        if (items[i].id != id)
            continue;

        items.removeAt(i);
        break;
    }

    activePopups.remove(id);
    commitReminderChanges();
}

void MainWindow::editReminderById(const QString &id)
{
    auto &items = store.items();
    for (Reminder &reminder : items)
    {
        if (reminder.id != id)
            continue;

        EditReminderValues values;
        if (!showEditReminderDialog(this, reminder, values))
            return;

        if (values.title.isEmpty())
        {
            QMessageBox::warning(this, "Invalid title", "Reminder title cannot be empty.");
            return;
        }

        reminder.title = values.title;
        reminder.repeating = values.repeating;

        if (values.scheduleType == ScheduleType::Relative)
        {
            int nextDelaySeconds = 0;
            QString err;
            if (!Parser::parseDurationToSeconds(values.relativeDelay, nextDelaySeconds, err))
            {
                QMessageBox::warning(this, "Invalid delay", err);
                return;
            }

            reminder.scheduleType = ScheduleType::Relative;
            reminder.nextLocal = QDateTime::currentDateTime().addSecs(nextDelaySeconds);

            if (!values.repeating)
            {
                reminder.intervalSeconds = nextDelaySeconds;
            }
            else
            {
                int repeatSeconds = nextDelaySeconds;
                if (!values.repeatInterval.isEmpty())
                {
                    if (!Parser::parseDurationToSeconds(values.repeatInterval, repeatSeconds, err))
                    {
                        QMessageBox::warning(this, "Invalid repeat interval", err);
                        return;
                    }
                }

                if (repeatSeconds < kShortRepeatWarningSeconds &&
                    !confirmShortRepeatInterval(repeatSeconds, "editing"))
                {
                    return;
                }

                reminder.intervalSeconds = repeatSeconds;
            }
        }
        else
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.timeOfDay = values.timeOfDay;
            reminder.nextLocal = nextAtTimeLocal(values.timeOfDay);
            reminder.intervalSeconds = 0;
        }

        commitReminderChanges();
        return;
    }
}

void MainWindow::refreshUI()
{
    store.sortSoonestFirst();
    const auto &items = store.items();
    countdownLabels.clear();
    list->clear();

    if (items.isEmpty())
    {
        stacked->setCurrentIndex(0);
    }
    else
    {
        stacked->setCurrentIndex(1);
        const QDateTime now = QDateTime::currentDateTime();

        for (const Reminder &reminder : items)
        {
            auto *row = new QWidget();
            row->setStyleSheet("background: #1a1a1a;");

            auto *root = new QHBoxLayout(row);
            root->setContentsMargins(18, 14, 18, 14);
            root->setSpacing(18);

            auto *leftTime = new QLabel(TimeFormat::formatCountdown(now.secsTo(reminder.nextLocal)));
            leftTime->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            leftTime->setStyleSheet("font-size: 32px; font-weight: 800; color: #eaeaea;");
            leftTime->setMinimumWidth(180);
            countdownLabels.insert(reminder.id, leftTime);

            auto *infoBox = new QVBoxLayout();
            infoBox->setContentsMargins(0, 0, 0, 0);
            infoBox->setSpacing(4);

            const QString titleText = reminder.repeating
                                          ? QString("%1  %2").arg(reminder.title, repeatIndicator())
                                          : reminder.title;
            auto *title = new QLabel(titleText);
            title->setStyleSheet("font-size: 16px; font-weight: 700;");

            auto *due = new QLabel(TimeFormat::formatDueDateTime(reminder.nextLocal));
            due->setStyleSheet("font-size: 13px; color: #bfbfbf;");

            auto *extra = new QLabel(repeatingInfoText(reminder));
            extra->setStyleSheet("font-size: 13px; color: #8f8f8f;");

            infoBox->addWidget(title);
            infoBox->addWidget(due);
            infoBox->addWidget(extra);

            auto *infoWrap = new QWidget();
            infoWrap->setLayout(infoBox);

            auto *actions = new QHBoxLayout();
            actions->setSpacing(10);
            actions->setContentsMargins(0, 0, 0, 0);

            auto *editBtn = new QPushButton("Edit");
            auto *delBtn = new QPushButton("Delete");
            editBtn->setStyleSheet("font-size: 13px; padding: 8px 12px;");
            delBtn->setStyleSheet("font-size: 13px; padding: 8px 12px;");

            connect(editBtn, &QPushButton::clicked, this, [this, rid = reminder.id]() { editReminderById(rid); });
            connect(delBtn, &QPushButton::clicked, this, [this, rid = reminder.id]() { deleteReminderById(rid); });

            actions->addWidget(editBtn);
            actions->addWidget(delBtn);

            auto *actionsWrap = new QWidget();
            actionsWrap->setLayout(actions);

            root->addWidget(leftTime);
            root->addWidget(infoWrap, 1);
            root->addWidget(actionsWrap);

            auto *item = new QListWidgetItem();
            item->setSizeHint(QSize(0, qMax(kMinimumRowHeight, row->sizeHint().height())));
            list->addItem(item);
            list->setItemWidget(item, row);
        }
    }

    refreshCompletedPreview();
}

void MainWindow::refreshCompletedPreview()
{
    completedList->clear();

    const auto &completed = store.completedItems();
    const bool hasCompleted = !completed.isEmpty();

    completedHeaderLabel->setVisible(hasCompleted);
    completedList->setVisible(hasCompleted);
    viewAllCompletedBtn->setEnabled(hasCompleted);

    if (!hasCompleted)
        return;

    const int start = qMax(0, completed.size() - kCompletedPreviewCount);
    for (int i = completed.size() - 1; i >= start; --i)
    {
        const CompletedReminder &entry = completed[i];

        auto *row = new QWidget();
        auto *root = new QHBoxLayout(row);
        root->setContentsMargins(12, 8, 12, 8);
        root->setSpacing(12);

        auto *textBox = new QVBoxLayout();
        textBox->setContentsMargins(0, 0, 0, 0);
        textBox->setSpacing(2);

        auto *title = new QLabel(completedTitleText(entry));
        title->setStyleSheet("font-size: 13px; font-weight: 700;");

        auto *meta = new QLabel(
            QString("%1 | %2")
                .arg(entry.completedAt.toString("M/d/yyyy h:mm AP"), completedPatternText(entry)));
        meta->setStyleSheet("font-size: 12px; color: #9f9f9f;");

        textBox->addWidget(title);
        textBox->addWidget(meta);

        auto *textWrap = new QWidget();
        textWrap->setLayout(textBox);

        auto *addAgainBtn = new QPushButton("Add Again");
        addAgainBtn->setStyleSheet("font-size: 12px; padding: 6px 10px;");
        connect(addAgainBtn, &QPushButton::clicked, this, [this, completedId = entry.id]()
                { reAddCompletedReminder(completedId); });

        root->addWidget(textWrap, 1);
        root->addWidget(addAgainBtn);

        auto *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, qMax(kCompletedPreviewRowHeight, row->sizeHint().height())));
        completedList->addItem(item);
        completedList->setItemWidget(item, row);
    }
}

void MainWindow::showAllCompletedDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Completed Reminders");
    dialog.resize(760, 480);
    dialog.setModal(true);

    auto *root = new QVBoxLayout(&dialog);
    auto *allList = new QListWidget(&dialog);
    allList->setStyleSheet(R"(
        QListWidget { background: #171717; border: 1px solid #2a2a2a; color: #eaeaea; }
        QListWidget::item { border-bottom: 1px solid #242424; }
    )");
    allList->setSpacing(0);
    allList->setUniformItemSizes(false);
    root->addWidget(allList, 1);

    const auto &completed = store.completedItems();
    for (int i = completed.size() - 1; i >= 0; --i)
    {
        const CompletedReminder &entry = completed[i];

        auto *row = new QWidget();
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);

        auto *textBox = new QVBoxLayout();
        textBox->setContentsMargins(0, 0, 0, 0);
        textBox->setSpacing(2);

        auto *title = new QLabel(completedTitleText(entry));
        title->setStyleSheet("font-size: 13px; font-weight: 700;");

        auto *meta = new QLabel(
            QString("%1 | %2")
                .arg(entry.completedAt.toString("M/d/yyyy h:mm AP"), completedPatternText(entry)));
        meta->setStyleSheet("font-size: 12px; color: #9f9f9f;");

        textBox->addWidget(title);
        textBox->addWidget(meta);

        auto *textWrap = new QWidget();
        textWrap->setLayout(textBox);

        auto *addAgainBtn = new QPushButton("Add Again");
        addAgainBtn->setStyleSheet("font-size: 12px; padding: 6px 10px;");
        connect(addAgainBtn, &QPushButton::clicked, this, [this, completedId = entry.id]()
                { reAddCompletedReminder(completedId); });

        layout->addWidget(textWrap, 1);
        layout->addWidget(addAgainBtn);

        auto *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, qMax(kCompletedPreviewRowHeight, row->sizeHint().height())));
        allList->addItem(item);
        allList->setItemWidget(item, row);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    root->addWidget(buttons);

    dialog.exec();
}

void MainWindow::reAddCompletedReminder(const QString &completedId)
{
    const auto &completed = store.completedItems();
    for (const CompletedReminder &entry : completed)
    {
        if (entry.id != completedId)
            continue;

        Reminder reminder;
        reminder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        reminder.title = entry.title;
        reminder.repeating = false;

        if (entry.scheduleType == ScheduleType::Relative)
        {
            const int interval = (entry.intervalSeconds > 0) ? entry.intervalSeconds : kDefaultReminderStepSeconds;
            reminder.scheduleType = ScheduleType::Relative;
            reminder.intervalSeconds = interval;
            reminder.nextLocal = QDateTime::currentDateTime().addSecs(interval);
        }
        else
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.timeOfDay = entry.timeOfDay.isValid() ? entry.timeOfDay : QTime::currentTime();
            reminder.nextLocal = nextAtTimeLocal(reminder.timeOfDay);
            reminder.intervalSeconds = 0;
        }

        store.items().push_back(reminder);
        commitReminderChanges();
        return;
    }
}

void MainWindow::appendCompletedReminder(const Reminder &reminder)
{
    if (reminder.repeating)
        return;

    auto &completedItems = store.completedItems();
    for (int i = 0; i < completedItems.size(); ++i)
    {
        if (!isSameCompletedPattern(completedItems[i], reminder))
            continue;

        CompletedReminder existing = completedItems.takeAt(i);
        existing.completionCount = std::max(1, existing.completionCount) + 1;
        existing.completedAt = QDateTime::currentDateTime();
        completedItems.push_back(existing);
        return;
    }

    CompletedReminder completed;
    completed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    completed.title = reminder.title;
    completed.scheduleType = reminder.scheduleType;
    completed.completedAt = QDateTime::currentDateTime();

    if (reminder.scheduleType == ScheduleType::Relative)
    {
        const int interval = (reminder.intervalSeconds > 0) ? reminder.intervalSeconds : kDefaultReminderStepSeconds;
        completed.intervalSeconds = interval;
    }
    else
    {
        completed.timeOfDay = reminder.timeOfDay;
    }

    completedItems.push_back(completed);
    while (completedItems.size() > kMaxCompletedItems)
        completedItems.removeFirst();
}

bool MainWindow::confirmShortRepeatInterval(int intervalSeconds, const QString &sourceLabel)
{
    if (intervalSeconds >= kShortRepeatWarningSeconds)
        return true;

    const QString msg =
        QString("This repeating reminder interval is very short (%1).\n"
                "That can trigger many popups very quickly.\n\n"
                "Continue %2 this reminder?")
            .arg(TimeFormat::formatIntervalText(intervalSeconds), sourceLabel);

    const QMessageBox::StandardButton result =
        QMessageBox::question(this, "Very Short Repeating Interval", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return result == QMessageBox::Yes;
}

void MainWindow::updateCountdownLabels()
{
    if (countdownLabels.isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    for (const Reminder &reminder : store.items())
    {
        QLabel *label = countdownLabels.value(reminder.id, nullptr);
        if (!label)
            continue;

        label->setText(TimeFormat::formatCountdown(now.secsTo(reminder.nextLocal)));
    }
}

void MainWindow::triggerDueReminders()
{
    const auto &items = store.items();
    if (items.isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    for (const Reminder &reminder : items)
    {
        if (reminder.nextLocal > now)
            continue;
        if (activePopups.contains(reminder.id))
            continue;

        activePopups.insert(reminder.id);
        WinFocus::bringToFront(this);

        auto *popup = new ReminderPopup(reminder.id, reminder.title, reminder.nextLocal, this);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        connect(popup, &ReminderPopup::okPressed, this, &MainWindow::handlePopupOk);
        connect(popup, &ReminderPopup::snoozePressed, this, &MainWindow::handlePopupSnooze);

        popup->show();
        WinFocus::bringToFront(popup);
    }
}

void MainWindow::handlePopupSnooze(const QString &id)
{
    const QDateTime now = QDateTime::currentDateTime();
    for (Reminder &reminder : store.items())
    {
        if (reminder.id != id)
            continue;

        reminder.nextLocal = now.addSecs(kSnoozeSeconds);
        break;
    }

    activePopups.remove(id);
    commitReminderChanges();
}

void MainWindow::handlePopupOk(const QString &id)
{
    const QDateTime now = QDateTime::currentDateTime();
    auto &items = store.items();

    for (int i = 0; i < items.size(); ++i)
    {
        Reminder &reminder = items[i];
        if (reminder.id != id)
            continue;

        if (!reminder.repeating)
        {
            appendCompletedReminder(reminder);
            items.removeAt(i);
        }
        else
        {
            rescheduleAfterAcknowledge(reminder, now);
        }
        break;
    }

    activePopups.remove(id);
    commitReminderChanges();
}

void MainWindow::saveStoreBestEffort()
{
    QString err;
    if (store.save(err))
    {
        saveErrorShown = false;
        return;
    }

    if (!saveErrorShown)
    {
        QMessageBox::warning(this, "Save error", err);
        saveErrorShown = true;
    }
}

void MainWindow::commitReminderChanges()
{
    store.sortSoonestFirst();
    saveStoreBestEffort();
    refreshUI();
}

