#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ReadingStatsTypes.h"

// The arithmetic behind the daily reading goal and the month calendar on the Reading Stats screen,
// on plain data so the host tests and the preview tool run it too. Days are the stats store's day
// index: days since 1970-01-01 in local time (localDayIndexFromEpoch), 0 meaning "unknown".
namespace ReadingCalendar {

struct Date {
  int year = 1970;
  int month = 1;  // 1-12
  int day = 1;    // 1-31
};

Date dateOf(uint16_t dayIndex);
// 0 when the date falls outside the store's uint16 window.
uint16_t dayIndexOf(int year, int month, int day);
// 0 = Monday ... 6 = Sunday.
int weekdayMondayFirst(uint16_t dayIndex);
int daysInMonth(int year, int month);
// (year, month) moved by `delta` months, either way.
void shiftMonth(int& year, int& month, int delta);

// The "Daily goal" setting's choices: Off, then minutes.
inline constexpr uint8_t kGoalChoiceCount = 7;
inline constexpr uint8_t kGoalMinutes[kGoalChoiceCount] = {0, 10, 15, 20, 30, 45, 60};
inline constexpr uint8_t kGoalDefaultChoice = 4;  // 30 min
uint8_t goalMinutes(uint8_t choice);

// How dark a day's calendar cell is, 0 (nothing read) to 3 (goal met). Without a goal, 30 minutes
// counts as a full day so the shades still mean something.
uint8_t shade(uint32_t seconds, uint8_t goalMinutes);

// The owner's average reading per calendar day over the `window` days ending `today` (fewer when
// the history is younger than that), in minutes. 0 when nothing is known.
uint16_t averageDailyMinutes(const std::vector<DayBucket>& days, uint16_t today, uint16_t window = 14);

// The minutes read on each day of a month, 31 entries (unused ones 0), from the store's sorted day
// buckets.
void monthMinutes(const std::vector<DayBucket>& days, int year, int month, uint16_t out[31]);

// Item `index` of a '|'-separated list ("January|February|..."), into `out`; false (and an empty
// string) past its end.
bool listItem(const char* list, int index, char* out, size_t size);

}  // namespace ReadingCalendar
