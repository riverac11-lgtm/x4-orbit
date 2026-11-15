#include <gtest/gtest.h>

#include "util/ReadingCalendar.h"

using namespace ReadingCalendar;

TEST(ReadingCalendar, DayIndexRoundTripsThroughDates) {
  EXPECT_EQ(dayIndexOf(1970, 1, 2), 1);
  const uint16_t nov15 = dayIndexOf(2026, 11, 15);
  ASSERT_NE(nov15, 0);
  const Date d = dateOf(nov15);
  EXPECT_EQ(d.year, 2026);
  EXPECT_EQ(d.month, 11);
  EXPECT_EQ(d.day, 15);
  for (uint16_t day = 1; day < 40000; day += 37) {
    const Date x = dateOf(day);
    EXPECT_EQ(dayIndexOf(x.year, x.month, x.day), day);
  }
  // Past the uint16 window.
  EXPECT_EQ(dayIndexOf(2200, 1, 1), 0);
}

TEST(ReadingCalendar, WeekdaysStartOnMonday) {
  EXPECT_EQ(weekdayMondayFirst(dayIndexOf(2026, 11, 15)), 6);  // a Sunday
  EXPECT_EQ(weekdayMondayFirst(dayIndexOf(2026, 11, 16)), 0);  // a Monday
  EXPECT_EQ(weekdayMondayFirst(dayIndexOf(2026, 11, 1)), 6);
}

TEST(ReadingCalendar, MonthLengthsAndShifts) {
  EXPECT_EQ(daysInMonth(2026, 2), 28);
  EXPECT_EQ(daysInMonth(2028, 2), 29);
  EXPECT_EQ(daysInMonth(2100, 2), 28);
  EXPECT_EQ(daysInMonth(2000, 2), 29);
  EXPECT_EQ(daysInMonth(2026, 11), 30);
  int y = 2026, m = 1;
  shiftMonth(y, m, -1);
  EXPECT_EQ(y, 2025);
  EXPECT_EQ(m, 12);
  shiftMonth(y, m, 14);
  EXPECT_EQ(y, 2027);
  EXPECT_EQ(m, 2);
}

TEST(ReadingCalendar, GoalChoicesAndShades) {
  EXPECT_EQ(goalMinutes(0), 0);
  EXPECT_EQ(goalMinutes(kGoalDefaultChoice), 30);
  EXPECT_EQ(goalMinutes(6), 60);
  EXPECT_EQ(goalMinutes(200), 0);

  EXPECT_EQ(shade(0, 20), 0);
  EXPECT_EQ(shade(59, 20), 0);
  EXPECT_EQ(shade(5 * 60, 20), 1);
  EXPECT_EQ(shade(10 * 60, 20), 2);
  EXPECT_EQ(shade(20 * 60, 20), 3);
  EXPECT_EQ(shade(90 * 60, 20), 3);
  // No goal: half an hour is a full day.
  EXPECT_EQ(shade(29 * 60, 0), 2);
  EXPECT_EQ(shade(30 * 60, 0), 3);
}

TEST(ReadingCalendar, AverageDailyMinutesSpreadsOverCalendarDays) {
  const uint16_t today = dayIndexOf(2026, 11, 15);
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

TEST(ReadingCalendar, MonthMinutesPicksTheMonthsBuckets) {
  const uint16_t first = dayIndexOf(2026, 11, 1);
  std::vector<DayBucket> days = {{static_cast<uint16_t>(first - 1), 600},
                                 {first, 125},
                                 {static_cast<uint16_t>(first + 29), 3600},
                                 {static_cast<uint16_t>(first + 30), 999}};
  uint16_t out[31];
  monthMinutes(days, 2026, 11, out);
  EXPECT_EQ(out[0], 2);
  EXPECT_EQ(out[29], 60);
  EXPECT_EQ(out[30], 0);  // December 1st is not November's
  int sum = 0;
  for (const uint16_t m : out) sum += m;
  EXPECT_EQ(sum, 62);
}

TEST(ReadingCalendar, ListItemsSplitOnBars) {
  char out[16];
  EXPECT_TRUE(listItem("January|February|March", 1, out, sizeof(out)));
  EXPECT_STREQ(out, "February");
  EXPECT_TRUE(listItem("January|February|March", 2, out, sizeof(out)));
  EXPECT_STREQ(out, "March");
  EXPECT_FALSE(listItem("January|February|March", 3, out, sizeof(out)));
  EXPECT_STREQ(out, "");
  EXPECT_TRUE(listItem("Septembrrrrrrrrrrrrrrr", 0, out, 6));
  EXPECT_STREQ(out, "Septe");
}
