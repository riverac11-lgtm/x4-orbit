#include "ChargeDelivery.h"

#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <InputManager.h>
#include <Logging.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_private/esp_clk.h>
#include <esp_sleep.h>
#include <soc/rtc.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "CrossPointSettings.h"
#include "OpdsServerStore.h"
#include "WifiCredentialStore.h"
#include "util/OrbitCatalogFiles.h"

namespace ChargeDelivery {

namespace {
// Survives deep sleep (the MCU stays powered), lost on a power-on: then it starts afresh.
RTC_DATA_ATTR ChargeSession::State rtcState;

// What the boot path may read without the card: 0 = off, else 1 + the interval setting.
constexpr char kNvsSpace[] = "Crosspoint";
constexpr char kNvsKey[] = "orbChg";
constexpr uint32_t kIntervalSec[] = {3600, 3 * 3600, 6 * 3600};

uint32_t rtcSeconds() {
  return static_cast<uint32_t>(rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000000ULL);
}

uint8_t readArmed() {
  Preferences nvs;
  if (!nvs.begin(kNvsSpace, true)) return 0;
  const uint8_t value = nvs.getUChar(kNvsKey, 0);
  nvs.end();
  return value;
}

bool powerPressed() { return digitalRead(InputManager::POWER_BUTTON_PIN) == LOW; }

// What keeps a headless session going, checked from the sync's hooks.
struct HeadlessCtx {
  HalGPIO* gpio;
  uint32_t startMs;
  uint32_t lastUsbCheckMs = 0;
  bool usb = true;
  bool power = false;
};

bool headlessKeepGoing(void* p) {
  auto& c = *static_cast<HeadlessCtx*>(p);
  if (c.power || powerPressed()) {
    c.power = true;
    return false;
  }
  const uint32_t now = millis();
  if (now - c.lastUsbCheckMs >= 1000) {  // an I2C read on the X3: not every call
    c.lastUsbCheckMs = now;
    c.usb = c.gpio->isUsbConnected();
  }
  return c.usb && now - c.startMs < kMaxSessionMs;
}

// Joins the saved networks, the last used first, each for up to 15 s.
bool connectSaved(HeadlessCtx& ctx) {
  const auto& creds = WIFI_STORE.getCredentials();
  if (creds.empty()) return false;
  const std::string& last = WIFI_STORE.getLastConnectedSsid();
  powerManager.ensureFullSpeedForRadio();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // The last network used first (pass 0), then the others in the order they were saved (pass 1).
  for (int pass = 0; pass < 2; ++pass) {
    for (const auto& cred : creds) {
      if ((cred.ssid == last) != (pass == 0)) continue;
      LOG_DBG("CHG", "Joining %s", cred.ssid.c_str());
      WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
      const uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        if (!headlessKeepGoing(&ctx)) return false;
        delay(200);
      }
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(false);
        return true;
      }
      WiFi.disconnect(false);
      delay(100);
    }
  }
  return false;
}

ChargeSession::Problem problemOf(const OrbitCatalogSync::Result result) {
  switch (result) {
    case OrbitCatalogSync::Result::Ok:
    case OrbitCatalogSync::Result::Stopped:
      return ChargeSession::Problem::None;
    case OrbitCatalogSync::Result::Auth:
      return ChargeSession::Problem::Auth;
    case OrbitCatalogSync::Result::Card:
      return ChargeSession::Problem::Card;
    case OrbitCatalogSync::Result::Server:
    case OrbitCatalogSync::Result::NoCatalogFeed:
      break;
  }
  return ChargeSession::Problem::Server;
}

void writeSummary(const ChargeSession::Summary& summary) {
  std::string text;
  ChargeSession::encodeSummary(summary, text);
  Storage.mkdir("/.crosspoint");
  HalFile file;
  if (!Storage.openFileForWrite("CHG", OrbitCatalogFiles::kSummaryPath, file)) return;
  file.write(text.data(), text.size());
  file.close();
}
}  // namespace

bool unseenSummary(ChargeSession::Summary& out) {
  if (!Storage.exists(OrbitCatalogFiles::kSummaryPath)) return false;
  std::string text;
  if (!Storage.readFileToString("CHG", OrbitCatalogFiles::kSummaryPath, 512, text)) return false;
  return ChargeSession::decodeSummary(text, out) && !out.shown && out.worthShowing();
}

void markSummarySeen(ChargeSession::Summary summary) {
  summary.shown = true;
  writeSummary(summary);
}

void describe(const ChargeSession::Summary& s, char* buf, const size_t size) {
  size_t n = 0;
  buf[0] = '\0';
  const auto line = [&](const char* text) {
    if (n >= size) return;
    const int w = snprintf(buf + n, size - n, "%s%s", n ? "\n" : "", text);
    if (w > 0) n += static_cast<size_t>(w);
  };
  char tmp[64];
  if (s.arrived > 0) {
    snprintf(tmp, sizeof(tmp), tr(STR_DELIVERY_ARRIVED), s.arrived);
    line(tmp);
  }
  if (s.failed > 0) {
    snprintf(tmp, sizeof(tmp), tr(STR_DELIVERY_FAILED), s.failed);
    line(tmp);
  }
  switch (s.problem) {
    case ChargeSession::Problem::Wifi:
      line(tr(STR_WIFI_CONN_FAILED));
      break;
    case ChargeSession::Problem::Server:
      line(tr(STR_DELIVERY_SERVER));
      break;
    case ChargeSession::Problem::Auth:
      line(tr(STR_AUTH_FAILED));
      break;
    case ChargeSession::Problem::Card:
      line(tr(STR_CATALOG_CARD));
      break;
    case ChargeSession::Problem::None:
      break;
  }
}

ChargeSession::Summary run(const OpdsServer& server, const OrbitCatalogSync::Hooks& hooks, const Work work,
                           const std::string& onlyId) {
  // The catalog is stamped with HalClock::now(), which stays 0 until the clock is trusted -- and a
  // cold boot or a stale restore leaves it untrusted while the TLS check (ensureUsableForTls)
  // passes, so nothing else syncs it: "Last updated" had no time to show. The reader is online
  // here, so ask NTP once (fast path ~20 ms, at most ~5 s), only when the time is unknown.
  if (work != Work::Download && !HalClock::isSynced()) HalClock::syncNtp(SETTINGS.ntpServer);
  ChargeSession::Summary summary;
  summary.when = static_cast<uint32_t>(HalClock::now());
  OrbitCatalogSync sync(server, hooks);
  OrbitCatalogSync::Result result = OrbitCatalogSync::Result::Ok;
  if (work != Work::Download) {
    int added = 0;
    result = sync.updateCatalog(work == Work::Build, added);
    summary.newInCatalog = static_cast<uint8_t>(added > 255 ? 255 : added);
    if (result == OrbitCatalogSync::Result::Ok && work != Work::Build) {
      int queued = 0;
      result = sync.collectSendToX4(queued);
    }
  }
  // A catalog that could not be updated does not keep the waiting books from coming.
  if ((work == Work::All || work == Work::Download) && result != OrbitCatalogSync::Result::Auth &&
      result != OrbitCatalogSync::Result::Stopped && result != OrbitCatalogSync::Result::Card) {
    const auto downloaded = sync.downloadWaiting(summary, onlyId);
    if (result == OrbitCatalogSync::Result::Ok) result = downloaded;
  }
  sync.closeSession();
  summary.problem = problemOf(result);
  if (auto buf = std::unique_ptr<char[]>(new (std::nothrow) char[OrbitCatalog::kMaxLine])) {
    const int waiting = OrbitCatalogFiles::waitingCount(server.url, buf.get());
    summary.waiting = static_cast<uint8_t>(waiting > 255 ? 255 : waiting);
  }
  LOG_INF("CHG", "Session: %d new in catalog, %d arrived, %d failed, %d waiting, problem %d", summary.newInCatalog,
          summary.arrived, summary.failed, summary.waiting, static_cast<int>(summary.problem));
  return summary;
}

void beforeSleep(const bool usbPresent) {
  uint8_t value = 0;
  if (SETTINGS.chargeDownload && OPDS_STORE.hasServers() && Storage.exists(WifiCredentialStore::FILE_PATH)) {
    value = static_cast<uint8_t>(1 + (SETTINGS.chargeCheckEvery < 3 ? SETTINGS.chargeCheckEvery : 1));
  }
  if (readArmed() != value) {  // NVS written only when it changes
    Preferences nvs;
    if (nvs.begin(kNvsSpace, false)) {
      nvs.putUChar(kNvsKey, value);
      nvs.end();
    }
  }
  armTimer(usbPresent);
}

void armTimer(const bool usbPresent) {
  const uint8_t armed = readArmed();
  const uint32_t interval = armed ? kIntervalSec[(armed - 1) % 3] : 0;
  const uint32_t sec = ChargeSession::nextWakeSec(rtcState, usbPresent, armed != 0, rtcSeconds(), interval);
  if (sec == 0) return;
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sec) * 1000000ULL);
  LOG_DBG("CHG", "Timer wake armed: %lu s", static_cast<unsigned long>(sec));
}

BootPlan planBoot(HalGPIO& gpio, const bool usbPlugBoot, const bool timerWake, const bool buttonHeld) {
  if ((!usbPlugBoot && !timerWake) || buttonHeld) return BootPlan::Normal;
  const bool armed = readArmed() != 0;
  if (usbPlugBoot && !armed) return BootPlan::Normal;  // the existing USB-power path, unchanged
  const bool usb = gpio.isUsbConnected();
  const auto plan = ChargeSession::plan(rtcState, timerWake ? ChargeSession::Wake::Timer : ChargeSession::Wake::UsbPlug,
                                        usb, armed, rtcSeconds());
  LOG_INF("CHG", "%s wake: usb=%d armed=%d -> %s", timerWake ? "Timer" : "USB", usb, armed,
          plan == ChargeSession::Plan::Run ? "run" : "sleep");
  return plan == ChargeSession::Plan::Run ? BootPlan::Run : BootPlan::Sleep;
}

bool runHeadless(HalGPIO& gpio) {
  HeadlessCtx ctx{&gpio, millis()};
  bool ok = false;
  // The clock first: a file is dated by it (main.cpp's order).
  HalClock::restore();
  if (Storage.begin()) {
    SETTINGS.loadFromFile();
    WIFI_STORE.loadFromFile();
    OPDS_STORE.loadFromFile();
    if (SETTINGS.chargeDownload && OPDS_STORE.hasServers()) {
      ChargeSession::Summary summary;
      if (!connectSaved(ctx)) {
        // Only worth a word when books were waiting for it (a charge away from home is not news).
        if (auto buf = std::unique_ptr<char[]>(new (std::nothrow) char[OrbitCatalog::kMaxLine])) {
          const int waiting = OrbitCatalogFiles::waitingCount(OPDS_STORE.getServers().front().url, buf.get());
          summary.waiting = static_cast<uint8_t>(waiting > 255 ? 255 : waiting);
        }
        if (summary.waiting > 0 && !ctx.power && ctx.usb) summary.problem = ChargeSession::Problem::Wifi;
      } else {
        HalClock::ensureUsableForTls(SETTINGS.ntpServer);
        OrbitCatalogSync::Hooks hooks;
        hooks.ctx = &ctx;
        hooks.keepGoing = &headlessKeepGoing;
        summary = run(OPDS_STORE.getServers().front(), hooks, Work::All);
        ok = summary.problem == ChargeSession::Problem::None && summary.failed == 0;
      }
      if (ctx.power || !ctx.usb) ok = false;  // cut short: the next charge tries again
      // Not worth a word (nothing arrived, nothing failed): written as already shown.
      summary.shown = !summary.worthShowing();
      ChargeSession::Summary previous;
      // An unseen summary is not replaced by an empty one.
      if (!(summary.shown && unseenSummary(previous))) writeSummary(summary);
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
    Storage.prepareForSleep();
  }
  ChargeSession::finished(rtcState, ok);
  LOG_INF("CHG", "Headless session ended after %lu ms (ok=%d power=%d usb=%d)",
          static_cast<unsigned long>(millis() - ctx.startMs), ok, ctx.power, ctx.usb);
  return ctx.power;
}

void logWakeStats() {
  if (rtcState.magic != ChargeSession::State::kMagic) return;
  LOG_INF("CHG", "Since power-on: %lu timer wakes, %lu charger boots, %lu sessions",
          static_cast<unsigned long>(rtcState.timerWakes), static_cast<unsigned long>(rtcState.plugWakes),
          static_cast<unsigned long>(rtcState.sessions));
}

}  // namespace ChargeDelivery
