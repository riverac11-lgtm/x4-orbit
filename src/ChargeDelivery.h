#pragma once

#include <cstdint>
#include <string>

#include "network/OrbitCatalogSync.h"
#include "util/ChargeSession.h"

class HalGPIO;

// Books delivered while charging (x4-orbit): with the reader on a charger, once per charge, it
// joins a saved Wi-Fi network, updates the saved catalog, puts the "Send to X4" collection's new
// books on the waiting list, downloads the waiting list, turns Wi-Fi off and sleeps again.
//
// When it runs. The reader is ASLEEP for it, and the panel is never touched (the sleep screen
// stays). What can wake it:
//  - X4 asleep with its battery latch cut (the clock setting off): the MCU has no power at all,
//    so plugging a charger in powers it up -- a cold boot main.cpp sees as AfterUSBPower, which
//    used to go straight back to sleep. That boot runs the session when one is due.
//  - Otherwise (X4 with the clock kept, X3, or a reader put to sleep while on the charger) the MCU
//    sleeps powered, and only a timer can wake it: neither board's USB detect is on a pin the C3
//    can wake from (X4: GPIO20; X3: the fuel gauge over I2C; deep-sleep GPIO wake is GPIO0-5
//    only). So a timer is armed at every sleep while the feature is on: every 1/3/6 h (setting)
//    to look for a charger, or after 30 s when the reader goes to sleep on a charger with a
//    session due. A timer wake that finds no charger is back asleep within a few hundred ms,
//    without the display or the SD card.
// Never on battery: the session stops if the charger goes, if the power button is pressed (the
// reader then wakes as usual), or after kMaxSessionMs.
namespace ChargeDelivery {

constexpr uint32_t kMaxSessionMs = 10 * 60 * 1000;

// ---- For the home screen (any theme) ----
// The last delivery's outcome when it is worth telling and has not been shown: arrived count and
// names, failures, a problem (Wi-Fi, server, password, card). Reads one small file on the card.
bool unseenSummary(ChargeSession::Summary& out);
// Shown: unseenSummary() is false until the next delivery.
void markSummarySeen();
// The summary as one or two plain lines ("New books arrived: 2" ...), for a popup or a home
// layout. `buf` gets both lines separated by '\n'.
void describe(const ChargeSession::Summary& summary, char* buf, size_t size);

// ---- A session, headless or on screen ----
enum class Work : uint8_t {
  Build,     // the catalog from scratch
  Update,    // the catalog brought up to date, and "Send to X4" onto the waiting list
  Download,  // the waiting list (or one book of it)
  All,       // Update, then Download: what the charger runs
};
// Wi-Fi must be up. Returns the outcome (arrived, failed, problem, still waiting).
ChargeSession::Summary run(const OpdsServer& server, const OrbitCatalogSync::Hooks& hooks, Work work,
                           const std::string& onlyId = {});

// ---- Sleep and boot (main.cpp) ----
// Before deep sleep from a running reader: records whether the feature can run (setting, a
// server, a saved network) where the next boot can read it without the card, then arms the
// timer. `usbPresent`: on a charger now.
void beforeSleep(bool usbPresent);
// Arms the timer from what beforeSleep() recorded: the early-boot sleep paths, which have no
// settings loaded.
void armTimer(bool usbPresent);

enum class BootPlan : uint8_t { Normal, Sleep, Run };
// Called right after the wake reason is known. `usbPlugBoot`: AfterUSBPower; `timerWake`: the
// deep-sleep timer woke it; `buttonHeld`: the power button is down (then always Normal).
BootPlan planBoot(HalGPIO& gpio, bool usbPlugBoot, bool timerWake, bool buttonHeld);
// The session without the display. True when the power button asked for a normal wake.
bool runHeadless(HalGPIO& gpio);
// The wake counters since power-on, logged at a normal boot (the battery cost, observable).
void logWakeStats();

}  // namespace ChargeDelivery
