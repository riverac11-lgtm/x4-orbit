#pragma once

// WitchReader SecureNet — the HTTP "Date:" response header as a clock source.
//
// Why: the X4 has no battery-backed clock, and with "Use Clock" off it stops while the reader
// sleeps, so it wakes behind. NTP fixes that when it gets through; when it does not (a network that
// drops UDP 123, a slow resolver), every reply from the reader's own book server still carries the
// server's idea of the time, to the second, in UTC. HalClock::applyHttpDate() decides whether to use
// it; this only reads it.
//
// Pure functions, no Arduino: host-tested in test/http_date.

#include <ctime>

namespace crosspoint {
namespace http_date {

// Parses an RFC 7231 / RFC 9110 IMF-fixdate, "Sun, 06 Nov 1994 08:49:37 GMT", to a UTC epoch.
// The day name, month name and "GMT" are matched case-sensitively as the RFC spells them; leading
// spaces are skipped. The obsolete RFC 850 and asctime forms are refused: no current server sends
// them, and refusing costs nothing but a missed clock hint. False (out untouched) for anything that
// is not a real date between 1970 and 2099.
bool parse(const char* value, time_t& out);

}  // namespace http_date
}  // namespace crosspoint
