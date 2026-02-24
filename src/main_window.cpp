#include "remindme/main_window.hpp"

#include "remindme/app_info.hpp"
#include "remindme/parser.hpp"
#include "remindme/reminder_popup.hpp"
#include "remindme/time_format.hpp"
#include "remindme/weekday_utils.hpp"
#include "remindme/win_focus.hpp"

#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QEasingCurve>
#include <QScreen>
#include <QStringConverter>
#include <QSystemTrayIcon>
#include <QTime>
#include <QTimeEdit>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <QWidget>
#include <QUuid>

#include <algorithm>
#include <array>
#include <functional>

namespace remindme
{

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
constexpr int kPopupTransitionDelayMs = 250;
constexpr int kPopupRefocusDelayMs = 150;
constexpr int kInitialDueCheckDelayMs = 250;
constexpr int kOverlayMaxVisibleTasks = 6;
constexpr int kCompletedSlideDurationMs = 180;
constexpr int kOverlayDefaultWidthPx = 280;
constexpr int kOverlayDefaultHeightPx = 380;
constexpr int kOverlayRootMarginPx = 0;
constexpr int kOverlayPanelMarginPx = 0;
constexpr int kOverlayRowPaddingLeftPx = 8;
constexpr int kOverlayRowPaddingRightPx = 8;
constexpr int kOverlayRowBorderPx = 2;
constexpr int kOverlayRowLayoutSafetyPx = 4;
constexpr int kOverlayTitleGapPx = 6;
constexpr int kOverlayMinTitleColumnWidthPx = 108;
constexpr int kOverlayMinTimerColumnWidthPx = 72;
constexpr int kOverlayMaxTimerColumnWidthPx = 132;
constexpr int kOverlayTitleInnerPaddingRightPx = 8;
constexpr int kOverlayTimerFontPixelSize = 14;
constexpr int kOverlayTextWidthSafetyPx = 6;
constexpr double kOverlayTitleColumnRatio = 0.65;
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
        const QString fileName = QString::fromLatin1(kGreetingFileName);
        QStringList candidates;
        candidates.push_back(QDir::current().filePath(fileName));

        QDir probe(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 4; ++i)
        {
            candidates.push_back(probe.filePath(fileName));
            if (!probe.cdUp())
                break;
        }

        for (const QString &path : candidates)
        {
            if (QFile::exists(path))
                return path;
        }
        return QCoreApplication::applicationDirPath() + "/" + fileName;
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

QDateTime nextAtTimeLocalFrom(const QTime &timeOfDay, const QDateTime &after, int repeatWeekdaysMask)
{
    const QDateTime anchor = after.isValid() ? after : QDateTime::currentDateTime();
    const int mask = WeekdayUtils::normalizeMask(repeatWeekdaysMask);
    const QDate baseDate = anchor.date();

    for (int dayOffset = 0; dayOffset <= 14; ++dayOffset)
    {
        const QDate candidateDate = baseDate.addDays(dayOffset);
        if (!WeekdayUtils::isSelected(mask, candidateDate.dayOfWeek()))
            continue;

        const QDateTime candidate(candidateDate, timeOfDay);
        if (candidate > anchor)
            return candidate;
    }

    return QDateTime(baseDate.addDays(1), timeOfDay);
}

QDateTime nextAtTimeLocal(const QTime &timeOfDay, int repeatWeekdaysMask = 0)
{
    return nextAtTimeLocalFrom(timeOfDay, QDateTime::currentDateTime(), repeatWeekdaysMask);
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

    const QString dayText = WeekdayUtils::maskDisplayText(reminder.repeatWeekdaysMask);
    if (dayText == "daily")
        return QString("At %1 daily").arg(reminder.timeOfDay.toString("h:mm AP"));
    return QString("At %1 on %2").arg(reminder.timeOfDay.toString("h:mm AP"), dayText);
}

QString subtaskProgressText(const Reminder &reminder)
{
    if (reminder.checklistItems.isEmpty())
        return QString();

    return QString("Subtasks %1/%2 done")
        .arg(reminder.checkedChecklistCount())
        .arg(reminder.checklistItems.size());
}

struct OverlayLayoutMetrics
{
    int contentWidthPx = 220;
    int titleColumnWidthPx = 142;
    int timerColumnWidthPx = 72;
};

void clearLayoutItems(QLayout *layout)
{
    if (!layout)
        return;

    while (QLayoutItem *item = layout->takeAt(0))
    {
        if (QLayout *childLayout = item->layout())
        {
            clearLayoutItems(childLayout);
            delete childLayout;
        }

        if (QWidget *childWidget = item->widget())
            childWidget->deleteLater();

        delete item;
    }
}

void installEventFilterRecursively(QWidget *root, QObject *eventFilterTarget)
{
    if (!root || !eventFilterTarget)
        return;

    root->installEventFilter(eventFilterTarget);
    const auto children = root->findChildren<QWidget *>();
    for (QWidget *child : children)
        child->installEventFilter(eventFilterTarget);
}

OverlayLayoutMetrics computeOverlayLayoutMetrics(int overlayWindowWidth)
{
    OverlayLayoutMetrics metrics;

    const int safeWindowWidth = qMax(220, overlayWindowWidth);
    const int outerChromePadding =
        (kOverlayRootMarginPx * 2) +
        (kOverlayPanelMarginPx * 2);
    const int rowChromePadding =
        kOverlayRowBorderPx +
        kOverlayRowLayoutSafetyPx +
        kOverlayRowPaddingLeftPx +
        kOverlayRowPaddingRightPx +
        kOverlayTitleGapPx +
        1; // divider line

    metrics.contentWidthPx = qMax(156, safeWindowWidth - outerChromePadding - rowChromePadding);

    const int compactTimerMin = qMax(52, kOverlayMinTimerColumnWidthPx - 16);
    const int compactTitleMin = qMax(84, kOverlayMinTitleColumnWidthPx - 18);

    int timerWidth = static_cast<int>(metrics.contentWidthPx * (1.0 - kOverlayTitleColumnRatio));
    timerWidth = qBound(compactTimerMin, timerWidth, kOverlayMaxTimerColumnWidthPx);

    int titleWidth = metrics.contentWidthPx - timerWidth;
    if (titleWidth < compactTitleMin)
    {
        titleWidth = compactTitleMin;
        timerWidth = qMax(compactTimerMin, metrics.contentWidthPx - titleWidth);
    }

    metrics.titleColumnWidthPx = qMax(64, titleWidth - kOverlayTitleInnerPaddingRightPx);
    metrics.timerColumnWidthPx = qMax(44, timerWidth);
    return metrics;
}

int overlayTitleFontSize(const QString &title, int titleWidthPx)
{
    QFont largeFont;
    largeFont.setPixelSize(16);
    largeFont.setBold(true);
    if (QFontMetrics(largeFont).horizontalAdvance(title) <= titleWidthPx)
        return 16;

    QFont mediumFont;
    mediumFont.setPixelSize(14);
    mediumFont.setBold(true);
    if (QFontMetrics(mediumFont).horizontalAdvance(title) <= titleWidthPx)
        return 14;

    return 13;
}

QString fitOverlayTitleText(const QString &rawTitle, int fontSizePx, int titleWidthPx)
{
    const QString title = rawTitle.simplified();
    const QString normalized = title.isEmpty() ? "(Untitled)" : title;

    QFont titleFont;
    titleFont.setPixelSize(fontSizePx);
    titleFont.setBold(true);
    const QFontMetrics metrics(titleFont);

    if (metrics.horizontalAdvance(normalized) <= titleWidthPx)
        return normalized;

    const int approxCharsPerLine = qBound(8, titleWidthPx / qMax(6, metrics.averageCharWidth()), 30);
    int splitPos = normalized.lastIndexOf(' ', approxCharsPerLine);
    if (splitPos <= 0 || splitPos >= normalized.size() - 1)
        return metrics.elidedText(normalized, Qt::ElideRight, titleWidthPx);

    const QString firstLine = metrics.elidedText(normalized.left(splitPos).trimmed(), Qt::ElideRight, titleWidthPx);
    const QString secondLine = metrics.elidedText(normalized.mid(splitPos + 1).trimmed(), Qt::ElideRight, titleWidthPx);
    return firstLine + "\n" + secondLine;
}

QString twoDigitsText(qint64 value)
{
    return QString("%1").arg(value, 2, 10, QChar('0'));
}

QString overlayDayToken(qint64 days)
{
    if (days > 9999)
        return "9999d+";
    return QString("%1d").arg(days);
}

QString fitOverlayCountdownText(qint64 secondsRemaining, int timerColumnWidthPx)
{
    const qint64 clampedSeconds = qMax<qint64>(0, secondsRemaining);
    qint64 remaining = clampedSeconds;
    const qint64 days = remaining / 86400;
    remaining %= 86400;
    const qint64 hours = remaining / 3600;
    remaining %= 3600;
    const qint64 minutes = remaining / 60;

    const QString fullText = TimeFormat::formatCountdown(clampedSeconds);
    QStringList candidates;
    candidates.push_back(fullText);

    if (days > 0)
    {
        candidates.push_back(QString("%1 %2:%3")
                                 .arg(overlayDayToken(days))
                                 .arg(twoDigitsText(hours))
                                 .arg(twoDigitsText(minutes)));
        candidates.push_back(overlayDayToken(days));
    }
    else
    {
        candidates.push_back(QString("%1:%2")
                                 .arg(twoDigitsText(hours))
                                 .arg(twoDigitsText(minutes)));
    }

    candidates.push_back(QString("%1m").arg((clampedSeconds + 59) / 60));
    candidates.push_back(QString("%1s").arg(clampedSeconds));

    QFont timerFont("Cascadia Mono");
    timerFont.setPixelSize(kOverlayTimerFontPixelSize);
    timerFont.setStyleHint(QFont::TypeWriter);
    timerFont.setBold(true);
    const QFontMetrics metrics(timerFont);
    const int maxTextWidthPx = qMax(24, timerColumnWidthPx - kOverlayTextWidthSafetyPx);
    for (const QString &candidate : candidates)
    {
        if (metrics.horizontalAdvance(candidate) <= maxTextWidthPx)
            return candidate;
    }

    const QString fallback = candidates.back();
    const int perCharPx = qMax(1, metrics.horizontalAdvance(QLatin1Char('8')));
    const int maxChars = qMax(2, maxTextWidthPx / perCharPx);
    if (fallback.size() <= maxChars)
        return fallback;
    return fallback.left(qMax(1, maxChars - 1)) + "+";
}

QWidget *createOverlayReminderRow(
    const Reminder &reminder,
    const QDateTime &now,
    const OverlayLayoutMetrics &layout,
    QWidget *parent,
    QObject *eventFilterTarget)
{
    auto *row = new QFrame(parent);
    row->setStyleSheet("background: #1a1a1a; border: 1px solid #2d2d2d; border-radius: 7px;");

    auto *root = new QVBoxLayout(row);
    root->setContentsMargins(8, 8, 8, 9);
    root->setSpacing(4);

    auto *top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(kOverlayTitleGapPx);

    const QString titleRaw = reminder.title.simplified();
    const int titleFontPx = overlayTitleFontSize(titleRaw, layout.titleColumnWidthPx);
    const QString titleText = fitOverlayTitleText(titleRaw, titleFontPx, layout.titleColumnWidthPx);

    auto *titleLabel = new QLabel(titleText, row);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setFixedWidth(layout.titleColumnWidthPx);
    titleLabel->setStyleSheet("color: #eaeaea;");
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(titleFontPx);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    const QFontMetrics titleMetrics(titleFont);
    titleLabel->setMaximumHeight((titleMetrics.lineSpacing() * 2) + 2);

    auto *divider = new QFrame(row);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setLineWidth(1);
    divider->setStyleSheet("color: #2a2a2a; background: #2a2a2a; border: none;");
    divider->setFixedHeight(titleLabel->maximumHeight());

    const QString timeText = fitOverlayCountdownText(now.secsTo(reminder.nextLocal), layout.timerColumnWidthPx);
    auto *timerLabel = new QLabel(timeText, row);
    timerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    timerLabel->setFixedWidth(layout.timerColumnWidthPx);
    timerLabel->setStyleSheet(
        "color: #f0f0f0; "
        "font-family: \"Cascadia Mono\", \"Consolas\", \"Lucida Console\", monospace; "
        "font-size: 14px; font-weight: 700;");

    top->addWidget(titleLabel, 0, Qt::AlignVCenter);
    top->addWidget(divider, 0, Qt::AlignVCenter);
    top->addWidget(timerLabel, 0, Qt::AlignVCenter);
    root->addLayout(top);

    const QString checklistText = subtaskProgressText(reminder);
    if (!checklistText.isEmpty())
    {
        auto *checklistLabel = new QLabel(checklistText, row);
        checklistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        checklistLabel->setFixedWidth(layout.titleColumnWidthPx);
        checklistLabel->setStyleSheet("font-size: 10px; color: #a8d6ff; border: none;");
        root->addWidget(checklistLabel, 0, Qt::AlignLeft);
    }

    installEventFilterRecursively(row, eventFilterTarget);
    return row;
}

Reminder *findReminderById(QVector<Reminder> &items, const QString &id)
{
    for (Reminder &item : items)
    {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

const Reminder *findReminderById(const QVector<Reminder> &items, const QString &id)
{
    for (const Reminder &item : items)
    {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

bool showChecklistDialog(QWidget *parent, const QString &reminderTitle, Reminder &reminder)
{
    if (!reminder.repeating)
        return false;

    QDialog dialog(parent);
    dialog.setWindowTitle(QString("Subtask - %1").arg(reminderTitle));
    dialog.setModal(true);
    dialog.resize(520, 420);

    auto *root = new QVBoxLayout(&dialog);
    auto *hint = new QLabel("Subtask progress resets each time this repeating reminder starts a new period.");
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 12px; color: #bdbdbd;");
    root->addWidget(hint);

    auto *checklist = new QListWidget(&dialog);
    checklist->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(checklist, 1);

    auto addChecklistRow = [&](const QString &text, bool checked)
    {
        const QString trimmedText = text.trimmed();
        if (trimmedText.isEmpty())
            return;

        auto *item = new QListWidgetItem(trimmedText);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        checklist->addItem(item);
    };

    for (const Reminder::ChecklistItem &item : reminder.checklistItems)
        addChecklistRow(item.text, item.checked);

    auto *addRow = new QHBoxLayout();
    auto *newItemEdit = new QLineEdit(&dialog);
    newItemEdit->setPlaceholderText("Add subtask item");
    auto *addBtn = new QPushButton("Add", &dialog);
    auto *removeBtn = new QPushButton("Remove Selected", &dialog);
    addRow->addWidget(newItemEdit, 1);
    addRow->addWidget(addBtn);
    addRow->addWidget(removeBtn);
    root->addLayout(addRow);

    QObject::connect(addBtn, &QPushButton::clicked, &dialog, [&]()
                     {
                         const QString text = newItemEdit->text().trimmed();
                         if (text.isEmpty())
                             return;

                         addChecklistRow(text, false);
                         newItemEdit->clear();
                         newItemEdit->setFocus(); });

    QObject::connect(newItemEdit, &QLineEdit::returnPressed, addBtn, &QPushButton::click);
    QObject::connect(removeBtn, &QPushButton::clicked, &dialog, [&]()
                     {
                         QListWidgetItem *selected = checklist->currentItem();
                         if (!selected)
                             return;
                         delete checklist->takeItem(checklist->row(selected)); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    QVector<Reminder::ChecklistItem> updatedChecklist;
    updatedChecklist.reserve(checklist->count());
    for (int i = 0; i < checklist->count(); ++i)
    {
        QListWidgetItem *item = checklist->item(i);
        if (!item)
            continue;

        const QString text = item->text().trimmed();
        if (text.isEmpty())
            continue;

        Reminder::ChecklistItem updated;
        updated.text = text;
        updated.checked = (item->checkState() == Qt::Checked);
        updatedChecklist.push_back(updated);
    }

    reminder.checklistItems = updatedChecklist;
    reminder.enforceChecklistConstraints();
    return true;
}

QString completedPatternText(const CompletedReminder &completed)
{
    if (completed.scheduleType == ScheduleType::Relative)
    {
        const int interval = (completed.intervalSeconds > 0) ? completed.intervalSeconds : kDefaultReminderStepSeconds;
        return QString("Pattern: in %1").arg(TimeFormat::formatIntervalText(interval));
    }

    const QString dayText = WeekdayUtils::maskDisplayText(completed.repeatWeekdaysMask);
    if (dayText == "daily")
        return QString("Pattern: at %1 daily").arg(completed.timeOfDay.toString("h:mm AP"));
    return QString("Pattern: at %1 on %2").arg(completed.timeOfDay.toString("h:mm AP"), dayText);
}

QString completedTitleText(const CompletedReminder &completed)
{
    if (completed.completionCount <= 1)
        return completed.title;

    return QString("%1  (x%2)").arg(completed.title).arg(completed.completionCount);
}

QWidget *createCompletedEntryRow(
    QWidget *parent,
    const CompletedReminder &entry,
    const std::function<void()> &onAddAgain)
{
    auto *row = new QWidget(parent);
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

    auto *textWrap = new QWidget(row);
    textWrap->setLayout(textBox);

    auto *addAgainBtn = new QPushButton("Add Again", row);
    addAgainBtn->setStyleSheet("font-size: 12px; padding: 6px 10px;");
    QObject::connect(addAgainBtn, &QPushButton::clicked, row, [onAddAgain]()
                     { onAddAgain(); });

    root->addWidget(textWrap, 1);
    root->addWidget(addAgainBtn);

    return row;
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

    return completed.timeOfDay == reminder.timeOfDay &&
           WeekdayUtils::normalizeMask(completed.repeatWeekdaysMask) == WeekdayUtils::normalizeMask(reminder.repeatWeekdaysMask);
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

    const int weekdayMask = reminder.repeating ? WeekdayUtils::normalizeMask(reminder.repeatWeekdaysMask) : 0;
    reminder.nextLocal = nextAtTimeLocalFrom(reminder.timeOfDay, now, weekdayMask);
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

    overlayToggle = new QCheckBox("Overlay");
    overlayToggle->setStyleSheet("font-size: 12px; color: #cfcfcf;");

    topRow->addWidget(titleLabel, 1);
    topRow->addWidget(overlayToggle, 0, Qt::AlignRight);
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

    completedSection = new QWidget();
    auto *completedSectionLayout = new QVBoxLayout(completedSection);
    completedSectionLayout->setContentsMargins(0, 0, 0, 0);
    completedSectionLayout->setSpacing(4);

    auto *completedHeaderRow = new QHBoxLayout();
    completedHeaderRow->setContentsMargins(0, 0, 0, 0);
    completedHeaderRow->setSpacing(8);

    completedHeaderLabel = new QLabel("Completed");
    completedHeaderLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #cccccc;");

    completedToggleBtn = new QToolButton();
    completedToggleBtn->setText("Hide");
    completedToggleBtn->setArrowType(Qt::DownArrow);
    completedToggleBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    completedToggleBtn->setAutoRaise(true);
    completedToggleBtn->setCursor(Qt::PointingHandCursor);
    completedToggleBtn->setStyleSheet("font-size: 12px; color: #cfcfcf; padding: 2px 4px;");
    connect(completedToggleBtn, &QToolButton::clicked, this, &MainWindow::toggleCompletedPreview);

    completedHeaderRow->addWidget(completedHeaderLabel);
    completedHeaderRow->addStretch(1);
    completedHeaderRow->addWidget(completedToggleBtn, 0, Qt::AlignRight);
    completedSectionLayout->addLayout(completedHeaderRow);

    completedPreviewBody = new QWidget();
    auto *completedBodyLayout = new QVBoxLayout(completedPreviewBody);
    completedBodyLayout->setContentsMargins(0, 0, 0, 0);
    completedBodyLayout->setSpacing(0);

    completedList = new QListWidget();
    completedList->setStyleSheet(R"(
        QListWidget { background: #151515; border: 1px solid #2a2a2a; }
        QListWidget::item { border-bottom: 1px solid #242424; }
    )");
    completedList->setSpacing(0);
    completedList->setUniformItemSizes(false);
    completedList->setFixedHeight((kCompletedPreviewCount * kCompletedPreviewRowHeight) + kCompletedPreviewListPadding);
    completedBodyLayout->addWidget(completedList);
    completedSectionLayout->addWidget(completedPreviewBody);
    v->addWidget(completedSection);

    completedPreviewAnim = new QPropertyAnimation(completedPreviewBody, "maximumHeight", this);
    completedPreviewAnim->setDuration(kCompletedSlideDurationMs);
    completedPreviewAnim->setEasingCurve(QEasingCurve::InOutCubic);
    completedPreviewBody->setMaximumHeight(completedPreviewExpandedHeight());
    setCompletedPreviewCollapsed(false, false);

    auto *h = new QHBoxLayout();
    input = new QLineEdit();
    input->setPlaceholderText(R"(e.g. "Drink water in 45m" or "Stand up at 7:00AM every day")");
    auto *exportBtn = new QPushButton("Export");
    auto *importBtn = new QPushButton("Import");
    exportBtn->setStyleSheet("font-size: 13px; padding: 8px 12px;");
    importBtn->setStyleSheet("font-size: 13px; padding: 8px 12px;");
    h->addWidget(input, 1);
    h->addWidget(exportBtn);
    h->addWidget(importBtn);
    v->addLayout(h);

    setCentralWidget(central);

    overlayWindow = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    overlayWindow->setAttribute(Qt::WA_QuitOnClose, false);
    overlayWindow->setAttribute(Qt::WA_TranslucentBackground, true);
    overlayWindow->setWindowTitle("RemindMe Overlay");
    overlayWindow->resize(kOverlayDefaultWidthPx, kOverlayDefaultHeightPx);
    overlayWindow->setObjectName("overlayWindow");
    overlayWindow->setStyleSheet(R"(
        QFrame#overlayPanel {
            background: transparent;
            border: none;
        }
    )");

    auto *overlayRootLayout = new QVBoxLayout(overlayWindow);
    overlayRootLayout->setContentsMargins(kOverlayRootMarginPx, kOverlayRootMarginPx, kOverlayRootMarginPx, kOverlayRootMarginPx);
    overlayRootLayout->setSpacing(0);

    auto *overlayPanel = new QFrame(overlayWindow);
    overlayPanel->setObjectName("overlayPanel");
    overlayRootLayout->addWidget(overlayPanel);

    auto *overlayLayout = new QVBoxLayout(overlayPanel);
    overlayLayout->setContentsMargins(kOverlayPanelMarginPx, kOverlayPanelMarginPx, kOverlayPanelMarginPx, kOverlayPanelMarginPx);
    overlayLayout->setSpacing(0);

    overlayBody = new QWidget(overlayPanel);
    overlayBody->setObjectName("overlayBody");
    overlayRowsLayout = new QVBoxLayout(overlayBody);
    overlayRowsLayout->setContentsMargins(0, 0, 0, 0);
    overlayRowsLayout->setSpacing(0);
    overlayLayout->addWidget(overlayBody, 1);
    installEventFilterRecursively(overlayWindow, this);

    if (QScreen *primaryScreen = QGuiApplication::primaryScreen())
    {
        const QRect available = primaryScreen->availableGeometry();
        overlayWindow->move(available.right() - overlayWindow->width() - 20, available.top() + 20);
    }
    overlayWindow->hide();

    connect(overlayToggle, &QCheckBox::toggled, this, &MainWindow::setOverlayVisible);

    connect(input, &QLineEdit::returnPressed, this, &MainWindow::onAddClicked);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::onImportClicked);

    QString err;
    if (!store.load(err))
        QMessageBox::warning(this, "Load error", err);

    setupSystemTray();

    updateGreetingMessage();
    nowLabel->setText(TimeFormat::formatClockTime(QDateTime::currentDateTime()));
    refreshUI();

    tickTimer = new QTimer(this);
    tickTimer->setInterval(kTickIntervalMs);
    connect(tickTimer, &QTimer::timeout, this, &MainWindow::onTick);
    tickTimer->start();

    QTimer::singleShot(kInitialDueCheckDelayMs, this, [this]()
                       { triggerDueReminders(); });
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (!quittingFromTray && trayIcon && trayIcon->isVisible())
    {
        saveStoreBestEffort();
        hide();
        e->ignore();

        if (!trayHintShown)
        {
            trayIcon->showMessage(
                AppInfo::kAppName,
                "RemindMe is still running in the system tray.",
                QSystemTrayIcon::Information,
                3000);
            trayHintShown = true;
        }
        return;
    }

    saveStoreBestEffort();
    QMainWindow::closeEvent(e);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!overlayWindow)
        return QMainWindow::eventFilter(watched, event);

    QWidget *watchedWidget = qobject_cast<QWidget *>(watched);
    if (!watchedWidget || watchedWidget->window() != overlayWindow)
        return QMainWindow::eventFilter(watched, event);

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton)
            break;

        if (QWindow *windowHandle = overlayWindow->windowHandle())
        {
            if (windowHandle->startSystemMove())
            {
                overlayDragging = false;
                return true;
            }
        }

        overlayDragging = true;
        overlayDragOffset = mouseEvent->globalPosition().toPoint() - overlayWindow->frameGeometry().topLeft();
        return true;
    }
    case QEvent::MouseMove:
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!overlayDragging || !(mouseEvent->buttons() & Qt::LeftButton))
            break;

        overlayWindow->move(mouseEvent->globalPosition().toPoint() - overlayDragOffset);
        return true;
    }
    case QEvent::MouseButtonRelease:
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && overlayDragging)
        {
            overlayDragging = false;
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    trayIcon = new QSystemTrayIcon(windowIcon(), this);
    trayIcon->setToolTip(AppInfo::kAppName);

    trayMenu = new QMenu(this);
    showAction = trayMenu->addAction("Open RemindMe");
    quitAction = trayMenu->addAction("Quit");

    connect(showAction, &QAction::triggered, this, &MainWindow::showFromTray);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();
}

void MainWindow::showFromTray()
{
    showNormal();
    WinFocus::bringToFront(this);
}

void MainWindow::quitFromTray()
{
    quittingFromTray = true;
    saveStoreBestEffort();

    setOverlayVisible(false);

    if (trayIcon)
        trayIcon->hide();

    close();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        showFromTray();
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
    updateOverlayContents();
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
    reminder.repeatWeekdaysMask = 0;

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
        const int repeatWeekdayMask = WeekdayUtils::normalizeMask(parsed.repeatWeekdaysMask);

        if (shouldRepeat && repeatWeekdayMask != 0)
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.intervalSeconds = 0;
            reminder.repeatWeekdaysMask = repeatWeekdayMask;
            reminder.nextLocal = nextAtTimeLocal(parsed.timeOfDay, reminder.repeatWeekdaysMask);
        }
        else if (shouldRepeat && parsed.hasRepeatDirective && parsed.repeatIntervalSeconds != 86400)
        {
            reminder.scheduleType = ScheduleType::Relative;
            reminder.intervalSeconds = parsed.repeatIntervalSeconds;
            reminder.nextLocal = QDateTime::currentDateTime().addSecs(parsed.repeatIntervalSeconds);
        }
        else
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.intervalSeconds = 0;
            reminder.repeatWeekdaysMask = 0;
            reminder.nextLocal = nextAtTimeLocal(parsed.timeOfDay);
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

void MainWindow::onExportClicked()
{
    QString err;
    const QString shareString = store.exportShareString(err);
    if (shareString.isEmpty())
    {
        QMessageBox::warning(this, "Export error", err.isEmpty() ? "Failed to create export string." : err);
        return;
    }

    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(shareString);

    QMessageBox info(this);
    info.setWindowTitle("Export Reminders");
    info.setIcon(QMessageBox::Information);
    info.setText("Export string copied to clipboard.");
    info.setInformativeText("Send this string to another RemindMe instance and use Import there.");
    info.setDetailedText(shareString);
    info.exec();
}

void MainWindow::onImportClicked()
{
    bool accepted = false;
    const QString shareString = QInputDialog::getMultiLineText(
        this,
        "Import Reminders",
        "Paste export string:",
        QString(),
        &accepted);

    if (!accepted)
        return;

    int importedCount = 0;
    QString err;
    if (!store.importShareString(shareString, importedCount, err))
    {
        QMessageBox::warning(this, "Import error", err);
        return;
    }

    commitReminderChanges();
    QMessageBox::information(this, "Import complete", QString("Imported %1 reminder(s).").arg(importedCount));
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

    queuedPopupIds.removeAll(id);
    if (activePopupId == id)
        activePopupId.clear();

    commitReminderChanges();
    queueNextPopupDisplay();
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
            reminder.repeatWeekdaysMask = 0;

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
            reminder.intervalSeconds = 0;
            if (!values.repeating)
            {
                reminder.repeatWeekdaysMask = 0;
            }
            else
            {
                reminder.repeatWeekdaysMask = WeekdayUtils::normalizeMask(reminder.repeatWeekdaysMask);
            }
            reminder.nextLocal = nextAtTimeLocal(values.timeOfDay, reminder.repeating ? reminder.repeatWeekdaysMask : 0);
        }

        reminder.enforceChecklistConstraints();
        queuedPopupIds.removeAll(id);
        commitReminderChanges();
        return;
    }
}

void MainWindow::editChecklistById(const QString &id)
{
    Reminder *reminder = findReminderById(store.items(), id);
    if (!reminder)
        return;

    if (!reminder->repeating)
    {
        QMessageBox::warning(this, "Subtask unavailable", "Subtasks are only available on repeating reminders.");
        return;
    }

    if (!showChecklistDialog(this, reminder->title, *reminder))
        return;

    commitReminderChanges();
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

            const QString extraText = repeatingInfoText(reminder);

            auto *extra = new QLabel(extraText);
            extra->setStyleSheet("font-size: 13px; color: #8f8f8f;");

            infoBox->addWidget(title);
            infoBox->addWidget(due);
            infoBox->addWidget(extra);

            if (!reminder.checklistItems.isEmpty())
            {
                auto *progressRow = new QHBoxLayout();
                progressRow->setContentsMargins(0, 0, 0, 0);
                progressRow->setSpacing(10);

                auto *progressLabel = new QLabel(subtaskProgressText(reminder));
                progressLabel->setStyleSheet("font-size: 12px; color: #a8d6ff;");

                auto *progress = new QProgressBar();
                progress->setRange(0, reminder.checklistItems.size());
                progress->setValue(reminder.checkedChecklistCount());
                progress->setTextVisible(false);
                progress->setFixedHeight(10);
                progress->setStyleSheet(R"(
                    QProgressBar {
                        border: 1px solid #3a3a3a;
                        background: #232323;
                        border-radius: 4px;
                    }
                    QProgressBar::chunk {
                        background: #4ca3ff;
                    }
                )");

                progressRow->addWidget(progressLabel);
                progressRow->addWidget(progress, 1);
                infoBox->addLayout(progressRow);

                bool hasPendingSubtasks = false;
                for (int subtaskIndex = 0; subtaskIndex < reminder.checklistItems.size(); ++subtaskIndex)
                {
                    const Reminder::ChecklistItem &subtask = reminder.checklistItems[subtaskIndex];
                    const QString subtaskText = subtask.text.trimmed();
                    if (subtask.checked || subtaskText.isEmpty())
                        continue;

                    if (!hasPendingSubtasks)
                    {
                        auto *subtaskHeader = new QLabel("Subtasks");
                        subtaskHeader->setStyleSheet("font-size: 12px; font-weight: 700; color: #d2d2d2;");
                        infoBox->addWidget(subtaskHeader);
                        hasPendingSubtasks = true;
                    }

                    auto *subtaskCheck = new QCheckBox(subtaskText);
                    subtaskCheck->setStyleSheet("font-size: 12px; color: #c0c0c0;");
                    subtaskCheck->setChecked(false);
                    connect(subtaskCheck, &QCheckBox::clicked, this, [this, rid = reminder.id, subtaskIndex](bool checked)
                            {
                                Reminder *target = findReminderById(store.items(), rid);
                                if (!target)
                                    return;
                                if (subtaskIndex < 0 || subtaskIndex >= target->checklistItems.size())
                                    return;
                                if (target->checklistItems[subtaskIndex].checked == checked)
                                    return;

                                target->checklistItems[subtaskIndex].checked = checked;
                                commitReminderChanges(); });
                    infoBox->addWidget(subtaskCheck);
                }

                if (!hasPendingSubtasks)
                {
                    auto *allDoneLabel = new QLabel("All subtasks done for this period.");
                    allDoneLabel->setStyleSheet("font-size: 12px; color: #8f8f8f;");
                    infoBox->addWidget(allDoneLabel);
                }
            }

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

            if (reminder.repeating)
            {
                auto *checklistBtn = new QPushButton("Subtask");
                checklistBtn->setStyleSheet("font-size: 13px; padding: 8px 12px;");
                connect(checklistBtn, &QPushButton::clicked, this, [this, rid = reminder.id]() { editChecklistById(rid); });
                actions->addWidget(checklistBtn);
            }
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
    updateOverlayContents();
}

void MainWindow::refreshCompletedPreview()
{
    completedList->clear();

    const auto &completed = store.completedItems();
    const bool hasCompleted = !completed.isEmpty();

    if (completedSection)
        completedSection->setVisible(hasCompleted);
    viewAllCompletedBtn->setEnabled(hasCompleted);

    if (!hasCompleted)
        return;

    const int start = qMax(0, completed.size() - kCompletedPreviewCount);
    for (int i = completed.size() - 1; i >= start; --i)
    {
        const CompletedReminder &entry = completed[i];
        auto *row = createCompletedEntryRow(
            completedList,
            entry,
            [this, completedId = entry.id]()
            { reAddCompletedReminder(completedId); });

        auto *item = new QListWidgetItem();
        item->setSizeHint(QSize(0, qMax(kCompletedPreviewRowHeight, row->sizeHint().height())));
        completedList->addItem(item);
        completedList->setItemWidget(item, row);
    }

    setCompletedPreviewCollapsed(completedPreviewCollapsed, false);
}

int MainWindow::completedPreviewExpandedHeight() const
{
    return (kCompletedPreviewCount * kCompletedPreviewRowHeight) + kCompletedPreviewListPadding;
}

void MainWindow::toggleCompletedPreview()
{
    setCompletedPreviewCollapsed(!completedPreviewCollapsed, true);
}

void MainWindow::setCompletedPreviewCollapsed(bool collapsed, bool animate)
{
    completedPreviewCollapsed = collapsed;
    if (!completedPreviewBody || !completedToggleBtn)
        return;

    completedToggleBtn->setText(collapsed ? "Show" : "Hide");
    completedToggleBtn->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);

    const int targetHeight = collapsed ? 0 : completedPreviewExpandedHeight();
    if (!completedPreviewAnim || !animate || !completedPreviewBody->isVisible())
    {
        if (completedPreviewAnim)
            completedPreviewAnim->stop();
        completedPreviewBody->setMaximumHeight(targetHeight);
        return;
    }

    completedPreviewAnim->stop();
    completedPreviewAnim->setStartValue(completedPreviewBody->maximumHeight());
    completedPreviewAnim->setEndValue(targetHeight);
    completedPreviewAnim->start();
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
        auto *row = createCompletedEntryRow(
            allList,
            entry,
            [this, completedId = entry.id]()
            { reAddCompletedReminder(completedId); });

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
            reminder.repeatWeekdaysMask = 0;
        }
        else
        {
            reminder.scheduleType = ScheduleType::AtTimeOfDay;
            reminder.timeOfDay = entry.timeOfDay.isValid() ? entry.timeOfDay : QTime::currentTime();
            reminder.repeatWeekdaysMask = WeekdayUtils::normalizeMask(entry.repeatWeekdaysMask);
            reminder.nextLocal = nextAtTimeLocal(reminder.timeOfDay, reminder.repeatWeekdaysMask);
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
        completed.repeatWeekdaysMask = 0;
    }
    else
    {
        completed.timeOfDay = reminder.timeOfDay;
        completed.repeatWeekdaysMask = WeekdayUtils::normalizeMask(reminder.repeatWeekdaysMask);
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

void MainWindow::updateOverlayContents()
{
    if (!overlayRowsLayout || !overlayBody)
        return;

    clearLayoutItems(overlayRowsLayout);

    const auto &items = store.items();
    if (items.isEmpty())
    {
        auto *emptyLabel = new QLabel("No remaining tasks.", overlayBody);
        emptyLabel->setStyleSheet("color: #a7a7a7; font-size: 13px; padding-top: 4px;");
        emptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        emptyLabel->installEventFilter(this);
        overlayRowsLayout->addWidget(emptyLabel);
        overlayRowsLayout->addStretch(1);
        return;
    }

    const int visibleCount = qMin(items.size(), kOverlayMaxVisibleTasks);
    const QDateTime now = QDateTime::currentDateTime();
    const int overlayWidth = overlayWindow ? overlayWindow->width() : kOverlayDefaultWidthPx;
    const OverlayLayoutMetrics layout = computeOverlayLayoutMetrics(overlayWidth);
    for (int i = 0; i < visibleCount; ++i)
    {
        overlayRowsLayout->addWidget(createOverlayReminderRow(items[i], now, layout, overlayBody, this));
        if (i < visibleCount - 1)
            overlayRowsLayout->addSpacing(7);
    }

    if (items.size() > visibleCount)
    {
        overlayRowsLayout->addSpacing(4);
        auto *moreLabel = new QLabel(QString("+%1 more task(s) not shown").arg(items.size() - visibleCount), overlayBody);
        moreLabel->setStyleSheet("color: #a7a7a7; font-size: 12px;");
        moreLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        moreLabel->installEventFilter(this);
        overlayRowsLayout->addWidget(moreLabel);
    }

    overlayRowsLayout->addStretch(1);
}

void MainWindow::setOverlayVisible(bool visible)
{
    overlayVisible = visible;

    if (overlayToggle && overlayToggle->isChecked() != visible)
        overlayToggle->setChecked(visible);

    if (!overlayWindow)
        return;

    if (!visible)
    {
        overlayDragging = false;
        overlayWindow->hide();
        return;
    }

    updateOverlayContents();
    overlayWindow->show();
    overlayWindow->raise();
}

void MainWindow::queueNextPopupDisplay()
{
    if (popupAdvanceQueued)
        return;

    popupAdvanceQueued = true;
    QTimer::singleShot(kPopupTransitionDelayMs, this, [this]()
                       {
                           popupAdvanceQueued = false;
                           showNextQueuedPopup();
                       });
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
        if (reminder.id == activePopupId)
            continue;
        if (queuedPopupIds.contains(reminder.id))
            continue;

        queuedPopupIds.push_back(reminder.id);
    }

    queueNextPopupDisplay();
}

void MainWindow::showNextQueuedPopup()
{
    if (activePopup)
        return;

    if (!activePopupId.isEmpty())
        activePopupId.clear();

    const auto &items = store.items();
    if (items.isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTime();

    while (!queuedPopupIds.isEmpty())
    {
        const QString reminderId = queuedPopupIds.takeFirst();
        const Reminder *reminder = findReminderById(items, reminderId);
        if (!reminder)
            continue;
        if (reminder->nextLocal > now)
            continue;

        activePopupId = reminderId;

        auto *popup = new ReminderPopup(reminder->id, reminder->title, reminder->nextLocal, nullptr);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        activePopup = popup;

        connect(popup, &QObject::destroyed, this, [this]()
                {
                    activePopup = nullptr;
                    if (activePopupId.isEmpty())
                        queueNextPopupDisplay();
                });
        connect(popup, &ReminderPopup::okPressed, this, &MainWindow::handlePopupOk);
        connect(popup, &ReminderPopup::snoozePressed, this, &MainWindow::handlePopupSnooze);
        popup->show();
        QTimer::singleShot(0, popup, [popup]()
                           { WinFocus::bringToFront(popup); });
        QTimer::singleShot(kPopupRefocusDelayMs, popup, [popup]()
                           { WinFocus::bringToFront(popup); });
        break;
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

    queuedPopupIds.removeAll(id);
    if (activePopupId == id)
        activePopupId.clear();

    commitReminderChanges();
    queueNextPopupDisplay();
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
            reminder.resetChecklist();
            rescheduleAfterAcknowledge(reminder, now);
        }
        break;
    }

    queuedPopupIds.removeAll(id);
    if (activePopupId == id)
        activePopupId.clear();

    commitReminderChanges();
    queueNextPopupDisplay();
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

}

