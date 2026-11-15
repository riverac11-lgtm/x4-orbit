#pragma once

#include <cstdint>

// "About how many days of reading are left on this charge": the battery drop measured over the
// last few wake/sleep cycles, per minute awake, times the owner's usual reading per day.
//
// Awake time, not the wall clock: the readers have no real-time clock, and a drain measured against
// a clock that may be unset or restored from a stale value would be wrong in both directions. The
// drain while asleep is small and lands in the next wake's reading, so it is counted against the
// awake minutes around it -- which is what a "days of reading" figure wants anyway.
//
// Pure data and arithmetic; BatteryLog (src/BatteryLog.h) keeps it on the card and feeds it.
namespace BatteryDays {

inline constexpr uint8_t kSamples = 16;        // 8 wake/sleep cycles
inline constexpr uint8_t kMinDropPercent = 2;  // below this the figure is noise
inline constexpr uint32_t kMinAwakeSeconds = 600;
inline constexpr uint8_t kMaxDays = 60;
inline constexpr uint8_t kNoiseRisePercent = 3;  // a bigger rise than this is a charge
inline constexpr uint8_t kDefaultDailyMinutes = 30;

struct Sample {
  uint8_t percent = 0;
  uint8_t reserved[3] = {};
  uint32_t awakeSeconds = 0;  // the log's running awake total when this was taken
};

struct Log {
  uint8_t count = 0;
  uint8_t dailyMinutes = 0;  // the last average the reading stats gave; 0 = not known yet
  uint8_t reserved[2] = {};
  uint32_t awakeSeconds = 0;  // running total of awake time since the log started
  Sample samples[kSamples];   // oldest first
};

void reset(Log& log);
// One battery reading at the current awake total. A rise past the noise margin means the battery
// was charged since the last reading: the measurement starts over from this one.
void record(Log& log, uint8_t percent);
void addAwake(Log& log, uint32_t seconds);
// A finished wake: the reading at wake, the time awake, the reading at sleep. `charged` (the cable
// was in at either end) starts the measurement over from the sleep reading.
void recordCycle(Log& log, uint8_t wakePercent, uint32_t awakeSeconds, uint8_t sleepPercent, bool charged);

// Whole days left at `percent` (1..kMaxDays), or 0 when there is not yet enough to tell.
// `dailyMinutes` 0 uses kDefaultDailyMinutes.
uint8_t estimateDays(const Log& log, uint8_t percent, uint16_t dailyMinutes);

// A log read from the card holds together (counts in range, samples in awake order).
bool valid(const Log& log);

}  // namespace BatteryDays
