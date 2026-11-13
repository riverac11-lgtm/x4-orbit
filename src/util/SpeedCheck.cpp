#include "SpeedCheck.h"

#include <Arduino.h>
#include <BoardConfig.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_heap_caps.h>

#include "BootDiagnostics.h"
#include "CrossPointSettings.h"
#include "SystemStatus.h"

namespace SpeedCheck {
namespace {
Log theLog;
}  // namespace

Log& log() { return theLog; }

void noteInput(const unsigned long eventMs, const unsigned long dispatchMs, const bool preDrawn) {
  theLog.noteInput(static_cast<uint32_t>(eventMs), static_cast<uint32_t>(dispatchMs), preDrawn);
}

void sectionEntry(const int spine) { theLog.sectionEntry(static_cast<uint32_t>(millis()), spine); }

void sectionBuilt(const int spine, const uint16_t pages) {
  theLog.sectionBuilt(static_cast<uint32_t>(millis()), spine, pages);
}

void refreshStart() { theLog.refreshStart(static_cast<uint32_t>(millis())); }

void pageVisible() { theLog.pageVisible(static_cast<uint32_t>(millis())); }

void bookOpened(const bool fromWake, const uint32_t loadMs, const uint32_t setupMs, const uint32_t sectionMs,
                const uint32_t drawMs, const uint32_t totalMs, const uint32_t pageAtMs) {
  OpenRecord record;
  record.fromWake = fromWake;
  record.loadMs = loadMs;
  record.setupMs = setupMs;
  record.sectionMs = sectionMs;
  record.drawMs = drawMs;
  record.totalMs = totalMs;
  // millis() starts with the app; the bootloader's share of a power-on wake is measured apart.
  record.sinceBootMs = pageAtMs + BootDiag::preAppMs();
  theLog.bookOpened(record);
}

size_t buildReport(char* out, const size_t cap, const bool withCardSize) {
  ReportEnv env;
  env.firmware = SystemStatus::firmwareVersion();
  env.board = BoardConfig::ACTIVE.name;
  // The settings that decide what the figures above can be: how often a page pays the slow
  // cleaning refresh, whether a second (grey) pass follows each page, and whether a page-turn
  // button waits out the double-click window (anything but 0 on a button adds 300 ms to it).
  char settings[112];
  snprintf(settings, sizeof(settings),
           "clean-refresh-every=%d pages, anti-aliasing=%u, half-after-image=%u, book-styles=%u, "
           "double-click L/R/up/down=%u/%u/%u/%u",
           SETTINGS.getRefreshFrequency(), SETTINGS.textAntiAliasing, SETTINGS.halfRefreshAfterImagePage,
           SETTINGS.embeddedStyle, SETTINGS.btnDoubleLeft, SETTINGS.btnDoubleRight, SETTINGS.btnDoublePageBack,
           SETTINGS.btnDoublePageForward);
  env.settings = settings;
  env.uptimeSeconds = millis() / 1000;
  env.cpuMhz = getCpuFrequencyMhz();
  env.cpuIdleMhz = HalPowerManager::LOW_POWER_FREQ;
  env.idleDownclockMs = HalPowerManager::IDLE_DOWNCLOCK_MS;
  env.lightSleepAfterMs = HalPowerManager::IDLE_LIGHT_SLEEP_MS;
  env.lightSleepSliceMs = HalPowerManager::LIGHT_SLEEP_SLICE_MS;
  const auto& sleep = powerManager.lightSleepStats();
  env.lightSleepSeconds = sleep.sleptMs / 1000;
  env.lightSleepSlices = sleep.slept;
  env.lightSleepDeclined = sleep.attempts - sleep.slept;
  env.usbPower = gpio.isUsbConnectedCached();
  env.batteryPercent = powerManager.getBatteryPercentage();
  env.freeHeap = esp_get_free_heap_size();
  if (withCardSize) {
    constexpr uint64_t kMb = 1024ULL * 1024ULL;
    env.sdTotalMb = static_cast<uint32_t>(Storage.sdTotalBytes() / kMb);
    env.sdFreeMb = static_cast<uint32_t>(Storage.sdFreeBytes() / kMb);
  }
  return formatReport(theLog, env, out, cap);
}

bool saveReport() {
  const auto text = makeUniqueNoThrow<char[]>(kReportBytes);
  if (!text) {
    LOG_ERR("SPD", "No memory for the speed report");
    return false;
  }
  const size_t length = buildReport(text.get(), kReportBytes, /*withCardSize=*/true);
  HalFile file;
  if (!Storage.openFileForWrite("SPD", kReportPath, file)) {
    return false;
  }
  // The write is timed and the figure appended: how fast this card takes a small file is one of
  // the things a slow book open can come down to.
  const unsigned long writeStart = millis();
  bool ok = file.write(text.get(), length) == length;
  file.flush();
  char tail[64];
  const int tailLength = snprintf(tail, sizeof(tail), "  this report: %u bytes written in %lu ms\n",
                                  static_cast<unsigned>(length), millis() - writeStart);
  ok = ok && tailLength > 0 && file.write(tail, static_cast<size_t>(tailLength)) == static_cast<size_t>(tailLength);
  ok = file.close() && ok;
  LOG_INF("SPD", "Speed report %s: %s", ok ? "saved" : "NOT saved", kReportPath);
  return ok;
}

}  // namespace SpeedCheck
