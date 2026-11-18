#pragma once

#include <cstdint>
#include <vector>

#include "ReadingStatsTypes.h"

// The owner's reading per calendar day, for the battery estimate (BatteryLog: "~9 days"). Days are
// the stats store's day index: days since 1970-01-01 in local time (localDayIndexFromEpoch), 0
// meaning "unknown". Reading Stats itself shows nothing by date any more (v3.2: the X4's clock
// stops while it sleeps).
namespace ReadingCalendar {

// The owner's average reading per calendar day over the `window` days ending `today` (fewer when
// the history is younger than that), in minutes. 0 when nothing is known.
uint16_t averageDailyMinutes(const std::vector<DayBucket>& days, uint16_t today, uint16_t window = 14);

}  // namespace ReadingCalendar
