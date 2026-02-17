#include "WinFocus.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace WinFocus
{
void bringToFront(QWidget *w)
{
    if (!w)
        return;

    w->showNormal();
    w->raise();
    w->activateWindow();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd)
        return;

    // Restore if minimized.
    ShowWindow(hwnd, SW_RESTORE);

    // Toggle topmost briefly to improve foreground behavior on Windows.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    SetForegroundWindow(hwnd);

    // Flash the taskbar icon to draw attention when focus is blocked.
    FLASHWINFO fi{};
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = hwnd;
    fi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
    fi.uCount = 6;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
#endif
}

} // namespace WinFocus
