#include <gtest/gtest.h>

#include "util/ReadingCalendar.h"

using namespace ReadingCalendar;

TEST(ReadingCalendar, AverageDailyMinutesSpreadsOverCalendarDays) {
  const uint16_t today = 20772;  // 2026-11-15
  // 14 days back: 70 min on two days -> 10 min a day over 14.
  std::vector<DayBucket> days = {
      {static_cast<uint16_t>(today - 20), 3600}, {static_cast<uint16_t>(today - 3), 40 * 60}, {today, 30 * 60}};
  EXPECT_EQ(averageDailyMinutes(days, today), 5);  // 70 min / 14 days
  // A young history counts only the days since it began.
  std::vector<DayBucket> young = {{static_cast<uint16_t>(today - 1), 40 * 60}, {today, 20 * 60}};
  EXPECT_EQ(averageDailyMinutes(young, today), 30);
  EXPECT_EQ(averageDailyMinutes({}, today), 0);
  EXPECT_EQ(averageDailyMinutes(days, 0), 0);
  // A bucket from "the future" (clock set back) is ignored.
  std::vector<DayBucket> future = {{static_cast<uint16_t>(today + 5), 600 * 60}};
  EXPECT_EQ(averageDailyMinutes(future, today), 0);
}
