#include "ReadingCalendar.h"

#include <algorithm>
#include <cstring>

namespace ReadingCalendar {

// Howard Hinnant's civil-from-days and days-from-civil, as ReadingStats.cpp's dayIndexFromLocaltime().
Date dateOf(const uint16_t dayIndex) {
  const long z = static_cast<long>(dayIndex) + 719468L;
  const long era = z / 146097L;
  const unsigned doe = static_cast<unsigned>(z - era * 146097L);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  Date d;
  d.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  d.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  d.year = static_cast<int>(yoe) + static_cast<int>(era) * 400 + (d.month <= 2 ? 1 : 0);
  return d;
}

uint16_t dayIndexOf(const int year, const int month, const int day) {
  const int y = year - (month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long days = era * 146097L + static_cast<long>(doe) - 719468L;
  if (days < 1 || days > 65535) return 0;
  return static_cast<uint16_t>(days);
}

int weekdayMondayFirst(const uint16_t dayIndex) { return (dayIndex + 3) % 7; }  // 1970-01-01 was a Thursday

int daysInMonth(const int year, const int month) {
  static constexpr uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return kDays[month - 1] + (month == 2 && leap ? 1 : 0);
}

void shiftMonth(int& year, int& month, const int delta) {
  int m = year * 12 + (month - 1) + delta;
  if (m < 0) m = 0;
  year = m / 12;
  month = m % 12 + 1;
}

uint8_t goalMinutes(const uint8_t choice) { return choice < kGoalChoiceCount ? kGoalMinutes[choice] : 0; }

uint8_t shade(const uint32_t seconds, const uint8_t goalMinutes) {
  if (seconds < 60) return 0;  // under a minute is a book opened, not a day read
  const uint32_t full = (goalMinutes ? goalMinutes : 30) * 60u;
  if (seconds >= full) return 3;
  return seconds * 2 >= full ? 2 : 1;
}

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

void monthMinutes(const std::vector<DayBucket>& days, const int year, const int month, uint16_t out[31]) {
  std::fill(out, out + 31, 0);
  const uint16_t first = dayIndexOf(year, month, 1);
  if (first == 0) return;
  const uint16_t last = static_cast<uint16_t>(first + daysInMonth(year, month) - 1);
  auto it = std::lower_bound(days.begin(), days.end(), first,
                             [](const DayBucket& b, const uint16_t v) { return b.dayIndex < v; });
  for (; it != days.end() && it->dayIndex <= last; ++it) {
    out[it->dayIndex - first] = static_cast<uint16_t>(std::min<uint32_t>(it->seconds / 60, 0xFFFF));
  }
}

bool listItem(const char* list, const int index, char* out, const size_t size) {
  if (size == 0) return false;
  out[0] = '\0';
  if (!list || index < 0) return false;
  const char* p = list;
  for (int i = 0; i < index; ++i) {
    p = std::strchr(p, '|');
    if (!p) return false;
    ++p;
  }
  const char* end = std::strchr(p, '|');
  const size_t n = std::min(end ? static_cast<size_t>(end - p) : std::strlen(p), size - 1);
  std::memcpy(out, p, n);
  out[n] = '\0';
  return true;
}

}  // namespace ReadingCalendar
