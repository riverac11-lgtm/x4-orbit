#pragma once

// Stub of lib/hal HalClock for SecureHttpClient's Date-header fallback: the test sets whether the
// clock wants a date and reads back what the client handed over.

#include <ctime>

struct FakeClock {
  static bool& approximate() {
    static bool v = false;
    return v;
  }
  static time_t& applied() {  // last epoch passed to applyHttpDate(), 0 when none
    static time_t v = 0;
    return v;
  }
};

namespace HalClock {
inline bool isApproximate() { return FakeClock::approximate(); }
inline bool isStaleRestore() { return false; }
inline bool applyHttpDate(time_t epoch) {
  FakeClock::applied() = epoch;
  return true;
}
}  // namespace HalClock
