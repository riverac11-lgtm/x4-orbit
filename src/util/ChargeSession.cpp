#include "ChargeSession.h"

#include <cstring>

namespace ChargeSession {

void reset(State& s) {
  s = State{};
  s.magic = State::kMagic;
}

namespace {
void ensureValid(State& s) {
  if (s.magic != State::kMagic) reset(s);
}

// The charger left: the next one is a new charge.
void chargerGone(State& s) {
  s.chargeDone = 0;
  s.failures = 0;
}

bool due(const State& s, const uint32_t nowSec) {
  return !s.chargeDone || s.lastRunSec == 0 || nowSec < s.lastRunSec || nowSec - s.lastRunSec >= kRepeatSec;
}
}  // namespace

Plan plan(State& s, const Wake wake, const bool usbPresent, const bool armed, const uint32_t nowSec) {
  ensureValid(s);
  if (wake == Wake::Timer) {
    ++s.timerWakes;
  } else {
    ++s.plugWakes;
    chargerGone(s);  // a cold boot on the charger is a charge of its own
  }
  if (!usbPresent) {
    chargerGone(s);
    return Plan::Sleep;
  }
  if (!armed || !due(s, nowSec)) return Plan::Sleep;
  ++s.sessions;
  s.lastRunSec = nowSec;
  return Plan::Run;
}

void finished(State& s, const bool ok) {
  ensureValid(s);
  if (ok) {
    s.chargeDone = 1;
    s.failures = 0;
    return;
  }
  if (++s.failures >= kMaxFailures) s.chargeDone = 1;
}

uint32_t nextWakeSec(State& s, const bool usbPresent, const bool armed, const uint32_t nowSec,
                     const uint32_t intervalSec) {
  ensureValid(s);
  if (!usbPresent) chargerGone(s);
  if (!armed) return 0;
  if (usbPresent && due(s, nowSec)) return s.failures > 0 ? kRetrySec : kSoonSec;
  return intervalSec;
}

void Summary::addName(std::string_view title) {
  if (nameCount >= kMaxNames) return;
  size_t n = title.size() < kNameLength - 1 ? title.size() : kNameLength - 1;
  // Never cut inside a UTF-8 sequence.
  while (n > 0 && n < title.size() && (static_cast<unsigned char>(title[n]) & 0xC0) == 0x80) --n;
  char* dst = names[nameCount++];
  for (size_t i = 0; i < n; ++i) dst[i] = (title[i] == '\n' || title[i] == '\t') ? ' ' : title[i];
  dst[n] = '\0';
}

namespace {
// The file is the struct itself behind a magic word: written and read by this firmware only, and
// a text format cost ~0.8 KB of flash for nothing a reader of the card would see.
constexpr uint32_t kSummaryMagic = 0x4F534D32;  // "OSM2"
struct Stored {
  uint32_t magic;
  Summary summary;
};
}  // namespace

void encodeSummary(const Summary& s, std::string& out) {
  Stored stored{kSummaryMagic, s};
  out.assign(reinterpret_cast<const char*>(&stored), sizeof(stored));
}

bool decodeSummary(std::string_view text, Summary& out) {
  out = Summary{};
  Stored stored{0, {}};
  if (text.size() != sizeof(stored)) return false;
  memcpy(&stored, text.data(), sizeof(stored));
  if (stored.magic != kSummaryMagic ||
      static_cast<uint8_t>(stored.summary.problem) > static_cast<uint8_t>(Problem::Card) ||
      stored.summary.nameCount > Summary::kMaxNames) {
    return false;
  }
  for (auto& name : stored.summary.names) name[Summary::kNameLength - 1] = '\0';
  out = stored.summary;
  return true;
}

}  // namespace ChargeSession
