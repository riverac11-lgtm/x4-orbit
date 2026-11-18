#include "ReadingCalendar.h"

#include <algorithm>

namespace ReadingCalendar {

uint16_t averageDailyMinutes(const std::vector<DayBucket>& days, const uint16_t today, const uint16_t window) {
  if (today == 0 || window == 0) return 0;
  // Days with a bucket newer than today (a clock that jumped back) are left out.
  uint16_t first = 0;
  for (const DayBucket& b : days) {
    if (b.dayIndex != 0 && b.dayIndex <= today) {
      first = b.dayIndex;
      break;
    }
  }
  if (first == 0) return 0;
  const uint16_t span = std::min<uint16_t>(window, static_cast<uint16_t>(today - first + 1));
  const uint16_t from = static_cast<uint16_t>(today - span + 1);
  uint32_t seconds = 0;
  for (const DayBucket& b : days) {
    if (b.dayIndex >= from && b.dayIndex <= today) seconds += b.seconds;
  }
  return static_cast<uint16_t>((seconds / span + 30) / 60);
}

}  // namespace ReadingCalendar
