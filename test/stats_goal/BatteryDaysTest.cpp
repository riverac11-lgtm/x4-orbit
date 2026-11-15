#include <gtest/gtest.h>

#include "util/BatteryDays.h"

using namespace BatteryDays;

TEST(BatteryDays, NothingUntilEnoughDrainIsSeen) {
  Log log;
  EXPECT_EQ(estimateDays(log, 90, 30), 0);
  recordCycle(log, 90, 30 * 60, 89, false);  // one percent: noise
  EXPECT_EQ(estimateDays(log, 89, 30), 0);
  recordCycle(log, 89, 5 * 60, 88, false);
  EXPECT_EQ(estimateDays(log, 88, 30), 51);  // 2% per 35 min awake; 88% at 30 min a day
}

TEST(BatteryDays, ShortWakesAreNotEnough) {
  Log log;
  recordCycle(log, 90, 60, 80, false);  // a big drop in a minute: a bad reading, not a rate
  EXPECT_EQ(estimateDays(log, 80, 30), 0);
}

TEST(BatteryDays, ScalesWithDailyReadingAndCaps) {
  Log log;
  recordCycle(log, 100, 60 * 60, 96, false);  // 4% an hour awake
  EXPECT_EQ(estimateDays(log, 96, 60), 24);   // 96% / 4% a day
  EXPECT_EQ(estimateDays(log, 96, 30), 48);
  EXPECT_EQ(estimateDays(log, 96, 10), kMaxDays);
  EXPECT_EQ(estimateDays(log, 96, 0), 48);  // unknown habit: 30 min
  EXPECT_EQ(estimateDays(log, 1, 600), 1);  // never "0 days" while there is charge
  EXPECT_EQ(estimateDays(log, 0, 30), 0);
}

TEST(BatteryDays, AChargeStartsOver) {
  Log log;
  recordCycle(log, 100, 60 * 60, 90, false);
  ASSERT_NE(estimateDays(log, 90, 30), 0);
  recordCycle(log, 90, 10 * 60, 100, true);  // on the cable
  EXPECT_EQ(log.count, 1);
  EXPECT_EQ(estimateDays(log, 100, 30), 0);

  Log rise;
  recordCycle(rise, 100, 60 * 60, 90, false);
  record(rise, 97);  // charged while asleep, without the cable seen
  EXPECT_EQ(rise.count, 1);

  Log noise;
  recordCycle(noise, 80, 60 * 60, 76, false);
  record(noise, 78);  // a battery recovering a little at rest is not a charge
  EXPECT_EQ(noise.count, 3);
}

TEST(BatteryDays, TheRingKeepsTheNewestSamples) {
  Log log;
  for (int i = 0; i < 20; ++i)
    recordCycle(log, static_cast<uint8_t>(100 - i), 600, static_cast<uint8_t>(99 - i), false);
  EXPECT_EQ(log.count, kSamples);
  EXPECT_EQ(log.samples[kSamples - 1].percent, 80);
  EXPECT_EQ(log.samples[0].percent, 88);
  EXPECT_TRUE(valid(log));
  // 8 cycles of 10 min, 8% gone: 1% per 10 min, 80% at 30 min a day = 27 days.
  EXPECT_EQ(estimateDays(log, 80, 30), 27);
}

TEST(BatteryDays, ResetKeepsTheDailyHabit) {
  Log log;
  log.dailyMinutes = 45;
  recordCycle(log, 100, 600, 90, true);
  EXPECT_EQ(log.dailyMinutes, 45);
}

TEST(BatteryDays, ValidRejectsDamagedLogs) {
  Log log;
  recordCycle(log, 100, 600, 95, false);
  EXPECT_TRUE(valid(log));
  Log bad = log;
  bad.count = kSamples + 1;
  EXPECT_FALSE(valid(bad));
  bad = log;
  bad.samples[0].percent = 180;
  EXPECT_FALSE(valid(bad));
  bad = log;
  bad.samples[1].awakeSeconds = bad.awakeSeconds + 1;
  EXPECT_FALSE(valid(bad));
}
