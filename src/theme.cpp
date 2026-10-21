#include "remindme/theme.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QSettings>

#include <utility>

namespace remindme
{

namespace
{
ThemePalette makeLight()
{
    ThemePalette p;
    p.dark = false;
    p.ground = "#F3F4F7";
    p.surface = "#FFFFFF";
    p.surfaceAlt = "#F7F8FA";
    p.border = "#DDE1E8";
    p.rule = "#ECEEF2";
    p.text = "#16213A";
    p.muted = "#566075";
    p.board = "#16213A";
    p.boardRaised = "#22305A";
    p.boardText = "#FFFFFF";
    p.boardMuted = "#B4BED2";
    p.accent = "#F5C84B";
    p.dueText = "#8A6100";
    p.danger = "#B3261E";
    p.focus = "#3B6FD8";
    p.primary = "#16213A";
    p.primaryHover = "#22305A";
    p.primaryText = "#FFFFFF";
    return p;
}

ThemePalette makeDark()
{
    ThemePalette p;
    p.dark = true;
    p.ground = "#10141C";
    p.surface = "#171C26";
    p.surfaceAlt = "#1C2230";
    p.border = "#2C3446";
    p.rule = "#232A39";
    p.text = "#E8ECF4";
    p.muted = "#A3ACBE";
    p.board = "#1B2742";
    p.boardRaised = "#26355A";
    p.boardText = "#FFFFFF";
    p.boardMuted = "#B4BED2";
    p.accent = "#F5C84B";
    p.dueText = "#F5C84B";
    p.danger = "#FF8A80";
    p.focus = "#7EA6FF";
    p.primary = "#2B3D66";
    p.primaryHover = "#364B7D";
    p.primaryText = "#FFFFFF";
    return p;
}

ThemePalette &currentPalette()
{
    static ThemePalette palette = makeLight();
    return palette;
}
}

bool Theme::systemPrefersDark()
{
    const QString forced = qEnvironmentVariable("REMINDME_THEME").trimmed().toLower();
    if (forced == "dark")
        return true;
    if (forced == "light")
        return false;

    const QSettings personalize(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        QSettings::NativeFormat);
    return personalize.value("AppsUseLightTheme", 1).toInt() == 0;
}

const ThemePalette &Theme::palette()
{
    return currentPalette();
}

void Theme::apply(bool dark)
{
    currentPalette() = dark ? makeDark() : makeLight();

    QFont font = QApplication::font();
    font.setFamilies({"Segoe UI Variable Text", "Segoe UI"});
    font.setPointSize(10);
    QApplication::setFont(font);

    // Style sheets can't reach placeholder text; the palette can.
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::PlaceholderText, QColor(currentPalette().muted));
    QApplication::setPalette(palette);

    if (auto *app = qobject_cast<QApplication *>(QApplication::instance()))
        app->setStyleSheet(styleSheet(currentPalette()));
}

QString Theme::styleSheet(const ThemePalette &p)
{
    QString css = R"(
QWidget { color: @text; }
QMainWindow, QDialog, QMessageBox, QInputDialog { background: @ground; }
QToolTip { background: @surface; color: @text; border: 1px solid @border; padding: 4px 6px; }

QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QDateTimeEdit {
    background: @surface; color: @text;
    border: 1px solid @border; border-radius: 6px; padding: 7px 10px;
    selection-background-color: @focus; selection-color: #FFFFFF;
    placeholder-text-color: @muted;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus, QDateTimeEdit:focus { border: 1px solid @focus; }
QComboBox QAbstractItemView { background: @surface; color: @text; border: 1px solid @border; selection-background-color: @surfaceAlt; selection-color: @text; }

QPushButton {
    background: @surface; color: @text;
    border: 1px solid @border; border-radius: 6px; padding: 7px 14px;
}
QPushButton:hover { background: @surfaceAlt; }
QPushButton:pressed { background: @rule; }
QPushButton:focus { border: 1px solid @focus; }
QPushButton:disabled { color: @muted; background: @ground; }
QPushButton#primaryButton { background: @primary; color: @primaryText; border: 1px solid @primary; font-weight: 600; padding: 8px 20px; }
QPushButton#primaryButton:hover { background: @primaryHover; border-color: @primaryHover; }
QPushButton#primaryButton:focus { border: 1px solid @focus; }

QPushButton[role="rowAction"] { background: transparent; border: 1px solid transparent; color: @muted; padding: 5px 10px; }
QPushButton[role="rowAction"]:hover { background: @surfaceAlt; border-color: @border; color: @text; }
QPushButton[role="rowAction"]:focus { border-color: @focus; color: @text; }
QPushButton[role="rowDanger"] { background: transparent; border: 1px solid transparent; color: @muted; padding: 5px 10px; }
QPushButton[role="rowDanger"]:hover { background: @surfaceAlt; border-color: @border; color: @danger; }
QPushButton[role="rowDanger"]:focus { border-color: @focus; color: @danger; }

QCheckBox { color: @text; spacing: 8px; }
QCheckBox[role="subtask"] { color: @muted; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid @muted; border-radius: 3px; background: @surface; }
QCheckBox::indicator:hover { border-color: @focus; }
QCheckBox::indicator:checked { background: @text; border-color: @text; }

QListWidget { background: @surface; border: 1px solid @border; border-radius: 8px; outline: none; }
QListWidget::item { border-bottom: 1px solid @rule; }
QListWidget::item:selected, QListWidget::item:hover { background: transparent; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 4px; min-height: 28px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

QMenu { background: @surface; color: @text; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 6px 22px 6px 14px; border-radius: 4px; }
QMenu::item:selected { background: @surfaceAlt; }
QMenu::separator { height: 1px; background: @rule; margin: 4px 6px; }


QLabel[role="muted"] { color: @muted; }
QLabel[role="hint"] { color: @muted; font-size: 9pt; }
QLabel[role="error"] { color: @danger; font-size: 9pt; }
QLabel[role="sectionTitle"] { color: @muted; font-size: 8pt; font-weight: 700; }

QLabel[role="colHeader"] { color: @muted; font-size: 8pt; font-weight: 700; }
QLabel[role="cellTime"] { color: @text; font-size: 11pt; }
QLabel[role="cellTitle"] { color: @text; font-size: 11pt; font-weight: 600; }
QLabel[role="cellMuted"] { color: @muted; font-size: 10pt; }
QLabel[role="cellCountdown"] { color: @text; font-size: 11pt; font-weight: 600; }
QLabel[role="cellDue"] { color: @dueText; font-size: 11pt; font-weight: 700; }

QFrame#board { background: @board; border: none; }
QFrame#board QLabel { color: @boardText; background: transparent; }
QFrame#board QLabel[role="boardMuted"] { color: @boardMuted; font-size: 9pt; }
QFrame#board QLabel[role="boardLabel"] { color: @accent; font-size: 8pt; font-weight: 700; }
QFrame#board QLabel[role="boardTitle"] { color: @boardText; font-size: 19pt; font-weight: 600; }
QFrame#board QLabel[role="boardCountdown"] { color: @accent; font-size: 40pt; font-weight: 600; }
QFrame#board QLabel[role="boardCountdownIdle"] { color: @boardMuted; font-size: 40pt; font-weight: 300; }
QFrame#board QPushButton, QFrame#board QToolButton {
    background: transparent; color: @boardText;
    border: 1px solid transparent; border-radius: 6px; padding: 6px 12px;
}
QFrame#board QPushButton:hover, QFrame#board QToolButton:hover { background: @boardRaised; }
QFrame#board QPushButton:focus, QFrame#board QToolButton:focus { border: 1px solid @accent; }
QFrame#board QPushButton:checked { background: @boardRaised; border: 1px solid @boardMuted; }
QFrame#board QPushButton:disabled { color: @boardMuted; }
QFrame#board QToolButton::menu-indicator { image: none; width: 0; }

QWidget#overlayWindow { background: transparent; }
QFrame[role="overlayPanel"] { background: @board; border: 1px solid @boardRaised; border-radius: 10px; }
QFrame[role="overlayPanel"] QLabel { background: transparent; border: none; }
QFrame[role="overlayRule"] { background: @boardRaised; border: none; }
QLabel[role="overlayTitle"] { color: @boardText; }
QLabel[role="overlayTime"] { color: @boardText; }
QLabel[role="overlayTimeNext"] { color: @accent; }
QLabel[role="overlayMeta"] { color: @boardMuted; font-size: 8pt; }

QDialog#reminderPopup { background: @ground; }
QFrame#popupBoard { background: @board; border: none; }
QFrame#popupBoard QLabel { color: @boardText; background: transparent; }
QFrame#popupBoard QLabel[role="boardLabel"] { color: @accent; font-size: 9pt; font-weight: 700; }
QFrame#popupBoard QLabel[role="boardMuted"] { color: @boardMuted; font-size: 11pt; }
QFrame#popupBoard QLabel[role="popupTitle"] { color: @boardText; font-size: 22pt; font-weight: 600; }
QLabel[role="popupDue"] { color: @dueText; font-size: 20pt; font-weight: 600; }
QLabel[role="popupClock"] { color: @muted; font-size: 10pt; }
QDialog#reminderPopup QPushButton { font-size: 12pt; padding: 10px 18px; }
)";

    const std::pair<const char *, const QString *> tokens[] = {
        {"@surfaceAlt", &p.surfaceAlt},
        {"@surface", &p.surface},
        {"@ground", &p.ground},
        {"@border", &p.border},
        {"@rule", &p.rule},
        {"@text", &p.text},
        {"@muted", &p.muted},
        {"@boardRaised", &p.boardRaised},
        {"@boardText", &p.boardText},
        {"@boardMuted", &p.boardMuted},
        {"@board", &p.board},
        {"@accent", &p.accent},
        {"@dueText", &p.dueText},
        {"@danger", &p.danger},
        {"@focus", &p.focus},
        {"@primaryHover", &p.primaryHover},
        {"@primaryText", &p.primaryText},
        {"@primary", &p.primary},
    };
    // Longer token names come before their prefixes (e.g. @surfaceAlt before @surface).
    for (const auto &[token, value] : tokens)
        css.replace(QString::fromLatin1(token), *value);

    return css;
}

}
