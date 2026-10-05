// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace remindme::WeekdayUtils
{

inline constexpr int kAllMask = 0x7F;
inline constexpr int kWeekdaysMask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
inline constexpr int kWeekendsMask = (1 << 5) | (1 << 6);

int weekdayBit(int dayOfWeek);
int normalizeMask(int rawMask);
bool isSelected(int weekdayMask, int dayOfWeek);
int weekdayFromToken(QString token);
QString maskDisplayText(int weekdayMask);

bool parseWeekdayMaskSpec(const QString &repeatSpec, int &outMask, QString &outError);

}

