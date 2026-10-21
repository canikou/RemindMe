#pragma once
#include <QString>

namespace remindme
{

// Departures Board palette. One set per Windows light/dark mode.
struct ThemePalette
{
    bool dark = false;

    QString ground;
    QString surface;
    QString surfaceAlt;
    QString border;
    QString rule;
    QString text;
    QString muted;

    QString board;
    QString boardRaised;
    QString boardText;
    QString boardMuted;
    QString accent;      // amber on the board: next countdown only
    QString dueText;     // amber readable on the ground: "Due now"
    QString danger;
    QString focus;
    QString primary;
    QString primaryHover;
    QString primaryText;
};

class Theme
{
public:
    // Follows the Windows app theme unless REMINDME_THEME=light|dark is set.
    static bool systemPrefersDark();
    static const ThemePalette &palette();

    // Sets the application font and style sheet; widgets restyle in place.
    static void apply(bool dark);
    static QString styleSheet(const ThemePalette &p);
};

}
