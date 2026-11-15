#include "ChargeSession.h"

#include <cstdio>
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

void encodeSummary(const Summary& s, std::string& out) {
  char buf[96];
  snprintf(buf, sizeof(buf), "S1\t%d\t%u\t%u\t%u\t%u\t%u\t%lu\n", s.shown ? 1 : 0, s.arrived, s.failed, s.waiting,
           s.newInCatalog, static_cast<unsigned>(s.problem), static_cast<unsigned long>(s.when));
  out = buf;
  for (uint8_t i = 0; i < s.nameCount && i < Summary::kMaxNames; ++i) {
    out += s.names[i];
    out += '\n';
  }
}

bool decodeSummary(std::string_view text, Summary& out) {
  out = Summary{};
  const size_t nl = text.find('\n');
  const std::string head(text.substr(0, nl));
  int shown = 0;
  unsigned arrived = 0, failed = 0, waiting = 0, fresh = 0, problem = 0;
  unsigned long when = 0;
  if (sscanf(head.c_str(), "S1\t%d\t%u\t%u\t%u\t%u\t%u\t%lu", &shown, &arrived, &failed, &waiting, &fresh, &problem,
             &when) != 7 ||
      problem > static_cast<unsigned>(Problem::Card)) {
    return false;
  }
  out.shown = shown != 0;
  out.arrived = static_cast<uint8_t>(arrived > 255 ? 255 : arrived);
  out.failed = static_cast<uint8_t>(failed > 255 ? 255 : failed);
  out.waiting = static_cast<uint8_t>(waiting > 255 ? 255 : waiting);
  out.newInCatalog = static_cast<uint8_t>(fresh > 255 ? 255 : fresh);
  out.problem = static_cast<Problem>(problem);
  out.when = static_cast<uint32_t>(when);
  size_t pos = nl == std::string_view::npos ? text.size() : nl + 1;
  while (pos < text.size() && out.nameCount < Summary::kMaxNames) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    if (end > pos) out.addName(text.substr(pos, end - pos));
    pos = end + 1;
  }
  return true;
}

}  // namespace ChargeSession
