#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ReadingStatsTypes.h"

// The arithmetic behind the Reading Stats screen and the Orbit homes' stats graphic, on plain data
// so the host tests run it. Nothing here reads a date: the X4's clock stops while it sleeps (Use
// Clock off), so every figure is built from reading time, pages and progress alone.
namespace ReadingInsights {

// ---- The recent sessions ---------------------------------------------------------------------
//
// The last kCapacity reading sessions, oldest first, kept in /.crosspoint/reading-sessions.bin
// (ReadingSessionLog). A session under kMinSeconds is a book opened and put down again, or a quick
// look: it is not logged, so it neither adds a stub of a bar nor drags the average down.
inline constexpr uint8_t kCapacity = 14;
inline constexpr uint32_t kMinSeconds = 60;
inline constexpr uint32_t kMagic = 0x31534552;  // "RES1", little-endian on the card

struct Session {
  uint32_t seconds = 0;
  uint32_t pages = 0;
};

// The file's image, written and read whole (~150 bytes).
struct SessionLog {
  uint32_t magic = kMagic;
  uint8_t count = 0;  // sessions held, at most kCapacity
  uint8_t reserved[3] = {};
  uint32_t check = 0;       // over everything below; a torn or foreign file reads as empty
  char lastDocId[36] = {};  // the book of the newest session, NUL-terminated
  Session sessions[kCapacity];
};

// Adds a session, dropping the oldest past kCapacity. False (and nothing changed) under kMinSeconds.
bool add(SessionLog& log, const char* docId, uint32_t seconds, uint32_t pages);
// Seals `log` for writing; valid() checks a log read back.
void seal(SessionLog& log);
bool valid(const SessionLog& log);
// The newest session, or nullptr while there is none.
const Session* newest(const SessionLog& log);
uint32_t averageSeconds(const SessionLog& log);  // 0 while there is none
uint32_t longestSeconds(const SessionLog& log);

// ---- The current book: time left ------------------------------------------------------------
//
// From the book's own seconds per percent read, so it follows this reader on this book. Shown only
// once the book has kEstimateMinSeconds of reading and kEstimateMinPercent of progress: before that
// a few seconds at 1% would promise a 20-minute novel.
inline constexpr uint32_t kEstimateMinSeconds = 600;
inline constexpr uint8_t kEstimateMinPercent = 2;

struct TimeLeft {
  bool known = false;
  uint32_t seconds = 0;
  uint16_t sessions = 0;  // at the average session length; 0 when that is not known
};
TimeLeft timeLeft(uint32_t bookSeconds, uint8_t progress, uint32_t averageSessionSeconds);

// ---- Milestones -----------------------------------------------------------------------------
//
// Hours 10, 25, 50, 100, 250, 500, 1000 ...; pages 1000, 2500, 5000, 10000 ...; books finished
// 5, 10, 15, 25, 50, 100, 250 ...
enum class Kind : uint8_t { Hours, Pages, Books };

struct Milestone {
  Kind kind = Kind::Hours;
  uint32_t value = 0;     // hours read (whole), pages turned, books finished
  uint32_t previous = 0;  // the threshold already passed (0 before the first)
  uint32_t next = 0;      // the next threshold, in the same unit
  uint32_t toGo = 0;      // seconds for Hours, else pages / books
  int permille = 0;       // the way from `previous` to `next`, 0-1000
};
// The smallest threshold above `value`.
uint32_t nextThreshold(Kind kind, uint32_t value);
// `amount` is seconds read for Hours, pages turned or books finished otherwise.
Milestone milestone(Kind kind, uint32_t amount);
// The one of `count` closest to done (the largest permille; the first of equals).
size_t closest(const Milestone* milestones, size_t count);

// ---- Top books --------------------------------------------------------------------------------
//
// One book can be in the history twice: the history is keyed by the file name, so a copy under a
// new name or a re-download starts a second entry. Entries with the same title and author (ASCII
// case ignored; never two untitled ones) become one, their time, pages and sessions summed, kept in
// order of time read, most first.
void mergeDuplicates(std::vector<BookReadingStats>& books);

// ---- Formatting --------------------------------------------------------------------------------
//
// "1 h 52 min" / "2 h" / "42 min" into `out`, with the caller's (translated) formats: `hm` takes
// hours and minutes, `h` hours, `m` minutes (under a minute: `m` with 0).
void formatDuration(char* out, size_t size, uint32_t seconds, const char* hm, const char* h, const char* m);
// 12345 -> "12,345".
void groupThousands(char* out, size_t size, uint32_t value);

}  // namespace ReadingInsights
