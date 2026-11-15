#include "BatteryLog.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "util/BatteryDays.h"

namespace {

constexpr uint32_t kMagic = 0x4C444231;  // "1BDL", little-endian on the card

struct FileImage {
  uint32_t magic = kMagic;
  BatteryDays::Log log;
};

BatteryDays::Log log_;
uint8_t wakePercent_ = 0;
bool cableAtWake_ = false;
bool started_ = false;

}  // namespace

void BatteryLog::begin(const uint8_t wakePercent, const bool cableAtWake) {
  wakePercent_ = wakePercent;
  cableAtWake_ = cableAtWake;
  started_ = true;
  FsFile file;
  if (!Storage.openFileForRead("BAT", kPath, file)) return;  // none yet: an empty log
  FileImage image;
  const int read = file.read(reinterpret_cast<uint8_t*>(&image), sizeof(image));
  file.close();
  if (read != static_cast<int>(sizeof(image)) || image.magic != kMagic || !BatteryDays::valid(image.log)) {
    LOG_DBG("BAT", "battery log unreadable; starting over");
    return;
  }
  log_ = image.log;
}

void BatteryLog::beforeSleep(const uint8_t sleepPercent, const bool cableNow, const uint32_t awakeSeconds) {
  if (!started_) return;  // a boot that never came far enough to read the battery
  BatteryDays::recordCycle(log_, wakePercent_, awakeSeconds, sleepPercent, cableAtWake_ || cableNow);
  FileImage image;
  image.log = log_;
  FsFile file;
  if (!Storage.openFileForWrite("BAT", kPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(&image), sizeof(image));
  file.close();
}

void BatteryLog::setDailyMinutes(const uint16_t minutes) {
  if (minutes > 0) log_.dailyMinutes = static_cast<uint8_t>(std::min<uint16_t>(minutes, 255));
}

uint8_t BatteryLog::daysLeft(const uint8_t percent, const bool cableIn) {
  if (cableIn) return 0;
  return BatteryDays::estimateDays(log_, percent, log_.dailyMinutes);
}

bool BatteryLog::format(char* out, const size_t size, const uint8_t percent, const bool cableIn) {
  if (size == 0) return false;
  out[0] = '\0';
  const uint8_t days = daysLeft(percent, cableIn);
  if (days == 0) return false;
  if (days == 1) {
    snprintf(out, size, "%s", tr(STR_BATTERY_ONE_DAY));
  } else {
    snprintf(out, size, tr(STR_BATTERY_DAYS), static_cast<int>(days));
  }
  return true;
}
