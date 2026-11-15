#include "BatteryDays.h"

namespace BatteryDays {

void reset(Log& log) {
  const uint8_t daily = log.dailyMinutes;
  log = Log{};
  log.dailyMinutes = daily;
}

void record(Log& log, const uint8_t percent) {
  if (percent > 100) return;
  if (log.count > 0 && percent > log.samples[log.count - 1].percent + kNoiseRisePercent) reset(log);
  if (log.count == kSamples) {
    for (uint8_t i = 1; i < kSamples; ++i) log.samples[i - 1] = log.samples[i];
    --log.count;
  }
  Sample& s = log.samples[log.count++];
  s = Sample{};
  s.percent = percent;
  s.awakeSeconds = log.awakeSeconds;
}

void addAwake(Log& log, const uint32_t seconds) { log.awakeSeconds += seconds; }

void recordCycle(Log& log, const uint8_t wakePercent, const uint32_t awakeSeconds, const uint8_t sleepPercent,
                 const bool charged) {
  if (charged) {
    reset(log);
    record(log, sleepPercent);
    return;
  }
  record(log, wakePercent);
  addAwake(log, awakeSeconds);
  record(log, sleepPercent);
}

uint8_t estimateDays(const Log& log, const uint8_t percent, const uint16_t dailyMinutes) {
  if (log.count < 2 || percent == 0) return 0;
  const Sample& first = log.samples[0];
  const Sample& last = log.samples[log.count - 1];
  if (first.percent < last.percent + kMinDropPercent) return 0;
  const uint32_t drop = first.percent - last.percent;
  const uint32_t awake = last.awakeSeconds - first.awakeSeconds;
  if (awake < kMinAwakeSeconds) return 0;
  const uint32_t daily = dailyMinutes ? dailyMinutes : kDefaultDailyMinutes;
  // percent / (drop / awake * 60 * daily), rounded, in integers: awake < 2^32, percent <= 100.
  const uint64_t num = static_cast<uint64_t>(percent) * awake;
  const uint64_t den = static_cast<uint64_t>(drop) * 60u * daily;
  const uint64_t days = (num + den / 2) / den;
  if (days < 1) return 1;
  return days > kMaxDays ? kMaxDays : static_cast<uint8_t>(days);
}

bool valid(const Log& log) {
  if (log.count > kSamples) return false;
  for (uint8_t i = 0; i < log.count; ++i) {
    if (log.samples[i].percent > 100) return false;
    if (log.samples[i].awakeSeconds > log.awakeSeconds) return false;
    if (i > 0 && log.samples[i].awakeSeconds < log.samples[i - 1].awakeSeconds) return false;
  }
  return true;
}

}  // namespace BatteryDays
