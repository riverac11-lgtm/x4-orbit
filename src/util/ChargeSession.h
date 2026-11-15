#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Books delivered while charging: when a session is due, when the reader should wake next, and the
// summary the next normal wake shows. Pure rules, no hardware (host tests: test/orbit_catalog);
// src/ChargeDelivery.cpp runs them at boot and before deep sleep.
namespace ChargeSession {

// Kept in RTC memory across deep sleep (lost on a power-on, which starts afresh). `magic` tells a
// valid state from the zeroes or garbage of a first boot.
struct State {
  static constexpr uint32_t kMagic = 0x0C4A5E01;
  uint32_t magic = 0;
  uint8_t chargeDone = 0;  // a session finished (or gave up) during the charge in progress
  uint8_t failures = 0;    // failed sessions in this charge
  uint32_t lastRunSec = 0;  // RTC seconds at the last session start
  // Counters, to show the extra wakes cost nothing: logged at each normal boot.
  uint32_t timerWakes = 0;
  uint32_t plugWakes = 0;
  uint32_t sessions = 0;
};

enum class Wake : uint8_t {
  Timer,    // the sleep timer this module armed
  UsbPlug,  // a cold boot that a charger caused (X4 asleep with its battery latch cut)
};
enum class Plan : uint8_t { Sleep, Run };

// A long charge (a reader that lives on its charger) is visited again after this long.
constexpr uint32_t kRepeatSec = 12 * 3600;
// A session that failed is retried at the next wake; after this many in one charge it waits for
// the next charge (or kRepeatSec).
constexpr uint8_t kMaxFailures = 3;
// Gone to sleep on the charger with a session due: wake for it this soon.
constexpr uint32_t kSoonSec = 30;
constexpr uint32_t kRetrySec = 15 * 60;

void reset(State& s);
// At a wake this module caused. `armed`: the feature is on and has a server and a network.
Plan plan(State& s, Wake wake, bool usbPresent, bool armed, uint32_t nowSec);
void finished(State& s, bool ok);
// Going to sleep: the timer to arm, in seconds, or 0 for none. `intervalSec` is the setting.
uint32_t nextWakeSec(State& s, bool usbPresent, bool armed, uint32_t nowSec, uint32_t intervalSec);

// The last session's outcome, kept on the card for the next normal wake to show once.
enum class Problem : uint8_t { None, Wifi, Server, Auth, Card };
struct Summary {
  static constexpr size_t kMaxNames = 3;
  static constexpr size_t kNameLength = 64;
  bool shown = false;
  uint8_t arrived = 0;
  uint8_t failed = 0;
  uint8_t waiting = 0;  // still waiting after the session
  uint8_t newInCatalog = 0;
  Problem problem = Problem::None;
  uint32_t when = 0;  // epoch seconds, 0 unknown
  uint8_t nameCount = 0;
  char names[kMaxNames][kNameLength] = {};
  void addName(std::string_view title);
  // Something the owner would want to hear about.
  bool worthShowing() const { return arrived > 0 || failed > 0 || problem != Problem::None; }
};
void encodeSummary(const Summary& s, std::string& out);
bool decodeSummary(std::string_view text, Summary& out);

}  // namespace ChargeSession
