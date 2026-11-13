#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

/// The pure half of the "Speed check": what the reader felt, kept in RAM, and how it is written out.
///
/// It answers three questions nobody could answer from the device itself: how long a book takes to
/// open, how long a chapter change takes, and how long a page turn takes -- each split far enough to
/// say where the time went (the card, the layout, the panel, or waiting for the reader to notice
/// the button).
///
/// Everything here is fixed-size and allocation-free: one book-open record, one chapter-change
/// record and a ring of the last kTurnCapacity page turns, about 300 bytes of .bss in all. The hot
/// path stores a few integers per page turn and never touches the card. Split from SpeedCheck.cpp
/// (millis(), the card, the settings) so the bookkeeping and the report text can be tested on the
/// host: this header depends on nothing but <cstdint> and <cstdio>.
///
/// Threading: the reader calls in from its loop task and its render task. All calls but
/// pageVisible() are made holding the RenderLock; pageVisible() runs after the lock was released
/// for the waveform and touches only the in-flight sample that refreshStart() set aside for it, and
/// the published records. A reader of the records can at worst see one half-written record, on a
/// screen that is not reachable while a page is being drawn.
namespace SpeedCheck {

inline constexpr uint8_t kTurnCapacity = 32;

/// Where the chapter's pages came from.
enum class Source : uint8_t {
  Unknown,
  Cached,     // the layout was on the card already
  Built,      // laid out first, then shown
  BuiltLive,  // the first page was shown while the rest of the chapter was still being laid out
};

struct SectionTiming {
  Source source = Source::Unknown;
  bool buildDone = false;  // buildMs and pages are known
  int16_t spine = -1;
  uint16_t pages = 0;
  uint32_t buildMs = 0;  // section entry to the end of the layout (wall time, drawing included)
};

struct OpenRecord {
  bool valid = false;
  bool fromWake = false;     // this open was the resume after a sleep
  uint32_t loadMs = 0;       // open request -> the book's index loaded (or built, on a first open)
  uint32_t setupMs = 0;      // -> the reader ready to draw (position, bookmarks, recent books, stats)
  uint32_t sectionMs = 0;    // -> the chapter loaded from its cache or laid out
  uint32_t drawMs = 0;       // -> the first page settled on the panel
  uint32_t totalMs = 0;      // open request -> first page settled
  uint32_t sinceBootMs = 0;  // fromWake: power-on -> first page settled
  SectionTiming section;
};

struct ChapterRecord {
  bool valid = false;
  uint32_t totalMs = 0;  // the turn or jump that left the old chapter -> first page settled
  SectionTiming section;
};

/// One page turn. 16-bit milliseconds, saturating: a turn that takes a minute is not a turn.
struct Turn {
  uint16_t waitMs = 0;     // button released -> the reader acted on it (poll slice, double-click window, busy loop)
  uint16_t renderMs = 0;   // -> the page handed to the panel (lock wait, page load, drawing)
  uint16_t refreshMs = 0;  // -> the panel settled
  uint8_t preDrawn = 0;    // the page was already drawn in the background (Background-A hit)
};

enum class TurnField : uint8_t { Wait, Render, Refresh, ToRefresh, Total };

struct Stats {
  uint8_t count = 0;
  uint16_t min = 0;
  uint16_t median = 0;
  uint16_t max = 0;
};

inline uint16_t clampMs(const uint32_t ms) { return ms > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(ms); }

class Log {
 public:
  // ---- book open -----------------------------------------------------------------
  /// A book open has started: whatever the reader was in the middle of timing is void, and the
  /// first section entry that follows belongs to the open, not to a chapter change.
  void beginOpen() {
    openPending_ = true;
    inputPending_ = false;
    sectionPending_ = false;
    inFlight_ = InFlight::None;
    awaiting_ = Awaiting::None;
  }

  /// The first page of the open is on the panel. `record` carries the phase times; the section
  /// timing is the tracker's own.
  void bookOpened(OpenRecord record) {
    if (!openPending_) return;
    openPending_ = false;
    record.valid = true;
    record.section = openSection_;
    open_ = record;
    openSection_ = {};
  }

  // ---- navigation input ----------------------------------------------------------
  /// A page turn was dispatched. `eventMs` is when the button was released (0 when the turn did
  /// not come from a button), `dispatchMs` when the reader started to act on it.
  void noteInput(const uint32_t eventMs, const uint32_t dispatchMs, const bool preDrawn) {
    inputPending_ = true;
    inputEventMs_ = (eventMs != 0 && dispatchMs - eventMs < kStaleMs) ? eventMs : dispatchMs;
    inputDispatchMs_ = dispatchMs;
    inputPreDrawn_ = preDrawn;
  }
  /// The turn went nowhere (the first page of the book): nothing will be drawn for it.
  void cancelInput() { inputPending_ = false; }

  // ---- section (chapter) entry ----------------------------------------------------
  void sectionEntry(const uint32_t nowMs, const int spine) {
    if (sectionPending_ && pending_.spine == spine) {
      return;  // the same entry again (a build that fell back to another path): keep its start
    }
    sectionPending_ = true;
    pending_ = {};
    pending_.spine = static_cast<int16_t>(spine);
    sectionEntryMs_ = nowMs;
    // A page turn that crossed into this chapter is the start the reader felt.
    sectionStartMs_ = (inputPending_ && nowMs - inputEventMs_ < kStaleMs) ? inputEventMs_ : nowMs;
    inputPending_ = false;
    awaiting_ = Awaiting::None;
  }

  void sectionProbed(const bool needBuild) {
    if (!sectionPending_) return;
    if (pending_.source != Source::Built) pending_.source = needBuild ? Source::Built : Source::Cached;
  }

  /// The chapter's layout is complete. Arrives before the first page for a build that blocks, and
  /// after it for one that shows pages as it goes: then it completes the record already published.
  void sectionBuilt(const uint32_t nowMs, const int spine, const uint16_t pages) {
    if (sectionPending_ && pending_.spine == spine) {
      finishBuild(pending_, nowMs, pages);
      return;
    }
    SectionTiming* published = awaiting_ == Awaiting::Open      ? (openPending_ ? &openSection_ : &open_.section)
                               : awaiting_ == Awaiting::Chapter ? &chapter_.section
                                                                : nullptr;
    if (published != nullptr && published->spine == spine) {
      finishBuild(*published, nowMs, pages);
    }
    awaiting_ = Awaiting::None;
  }

  // ---- display ---------------------------------------------------------------------
  /// A page is about to be handed to the panel. Sets aside what this refresh answers, so an input
  /// that arrives during the waveform is not mistaken for the one it belongs to.
  void refreshStart(const uint32_t nowMs) {
    refreshStartMs_ = nowMs;
    if (sectionPending_) {
      inFlight_ = InFlight::Section;
    } else if (inputPending_ && nowMs - inputDispatchMs_ < kStaleMs) {
      inFlight_ = InFlight::Turn;
      flightEventMs_ = inputEventMs_;
      flightDispatchMs_ = inputDispatchMs_;
      flightPreDrawn_ = inputPreDrawn_;
    } else {
      inFlight_ = InFlight::None;  // a status-bar tick, a redraw after a menu
    }
    inputPending_ = false;
  }

  /// The page handed over by the last refreshStart() has settled on the panel.
  void pageVisible(const uint32_t nowMs) {
    const InFlight kind = inFlight_;
    inFlight_ = InFlight::None;
    if (kind == InFlight::Turn) {
      Turn turn;
      turn.waitMs = clampMs(flightDispatchMs_ - flightEventMs_);
      turn.renderMs = clampMs(refreshStartMs_ - flightDispatchMs_);
      turn.refreshMs = clampMs(nowMs - refreshStartMs_);
      turn.preDrawn = flightPreDrawn_ ? 1 : 0;
      turns_[turnNext_] = turn;
      turnNext_ = static_cast<uint8_t>((turnNext_ + 1) % kTurnCapacity);
      if (turnCount_ < kTurnCapacity) turnCount_++;
      return;
    }
    if (kind != InFlight::Section || !sectionPending_) return;
    sectionPending_ = false;
    if (pending_.source == Source::Built && !pending_.buildDone) pending_.source = Source::BuiltLive;
    const bool buildOwed = pending_.source == Source::BuiltLive;
    if (openPending_) {
      openSection_ = pending_;
      awaiting_ = buildOwed ? Awaiting::Open : Awaiting::None;
      return;
    }
    chapter_.valid = true;
    chapter_.totalMs = nowMs - sectionStartMs_;
    chapter_.section = pending_;
    awaiting_ = buildOwed ? Awaiting::Chapter : Awaiting::None;
  }

  // ---- reading the records ----------------------------------------------------------
  const OpenRecord& lastOpen() const { return open_; }
  const ChapterRecord& lastChapter() const { return chapter_; }
  uint8_t turnCount() const { return turnCount_; }
  /// i = 0 is the oldest turn kept.
  const Turn& turn(const uint8_t i) const {
    const uint8_t first = turnCount_ < kTurnCapacity ? 0 : turnNext_;
    return turns_[(first + i) % kTurnCapacity];
  }
  uint8_t preDrawnTurns() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < turnCount_; i++) n = static_cast<uint8_t>(n + turns_[i].preDrawn);
    return n;
  }

  static uint16_t fieldOf(const Turn& t, const TurnField field) {
    switch (field) {
      case TurnField::Wait:
        return t.waitMs;
      case TurnField::Render:
        return t.renderMs;
      case TurnField::Refresh:
        return t.refreshMs;
      case TurnField::ToRefresh:
        return clampMs(static_cast<uint32_t>(t.waitMs) + t.renderMs);
      case TurnField::Total:
        break;
    }
    return clampMs(static_cast<uint32_t>(t.waitMs) + t.renderMs + t.refreshMs);
  }

  /// Min, median and max of one figure over the turns kept. The median of an even count is the
  /// lower of the two middle values: a time that was measured, not an average of two.
  Stats turnStats(const TurnField field) const {
    Stats s;
    s.count = turnCount_;
    if (turnCount_ == 0) return s;
    uint16_t sorted[kTurnCapacity];
    for (uint8_t i = 0; i < turnCount_; i++) {
      const uint16_t v = fieldOf(turns_[i], field);
      uint8_t j = i;
      while (j > 0 && sorted[j - 1] > v) {
        sorted[j] = sorted[j - 1];
        j--;
      }
      sorted[j] = v;
    }
    s.min = sorted[0];
    s.max = sorted[turnCount_ - 1];
    s.median = sorted[(turnCount_ - 1) / 2];
    return s;
  }

 private:
  // An input older than this when its page is drawn was not what the draw answered.
  static constexpr uint32_t kStaleMs = 60000;

  enum class InFlight : uint8_t { None, Turn, Section };
  enum class Awaiting : uint8_t { None, Open, Chapter };

  void finishBuild(SectionTiming& timing, const uint32_t nowMs, const uint16_t pages) const {
    timing.buildDone = true;
    timing.buildMs = nowMs - sectionEntryMs_;
    timing.pages = pages;
    if (timing.source == Source::Unknown || timing.source == Source::Cached) timing.source = Source::Built;
  }

  OpenRecord open_;
  ChapterRecord chapter_;
  SectionTiming openSection_;
  SectionTiming pending_;
  Turn turns_[kTurnCapacity] = {};
  uint32_t inputEventMs_ = 0;
  uint32_t inputDispatchMs_ = 0;
  uint32_t flightEventMs_ = 0;
  uint32_t flightDispatchMs_ = 0;
  uint32_t sectionEntryMs_ = 0;
  uint32_t sectionStartMs_ = 0;
  uint32_t refreshStartMs_ = 0;
  uint8_t turnNext_ = 0;
  uint8_t turnCount_ = 0;
  InFlight inFlight_ = InFlight::None;
  Awaiting awaiting_ = Awaiting::None;
  bool openPending_ = false;
  bool inputPending_ = false;
  bool inputPreDrawn_ = false;
  bool flightPreDrawn_ = false;
  bool sectionPending_ = false;
};

// ---------------------------------------------------------------------------------------
// Report text
// ---------------------------------------------------------------------------------------

/// What the report says about the device, gathered by the caller: this header reads no hardware.
struct ReportEnv {
  const char* firmware = "";
  const char* board = "";
  const char* settings = "";  // one line, already formatted
  uint32_t uptimeSeconds = 0;
  uint32_t cpuMhz = 0;
  uint32_t cpuIdleMhz = 0;
  uint32_t idleDownclockMs = 0;
  uint32_t lightSleepAfterMs = 0;
  uint32_t lightSleepSliceMs = 0;
  uint32_t lightSleepSeconds = 0;
  uint32_t lightSleepSlices = 0;
  uint32_t lightSleepDeclined = 0;  // attempts that did not sleep (a render, Wi-Fi, USB, a button mid-debounce)
  bool usbPower = false;
  uint16_t batteryPercent = 0;
  uint32_t freeHeap = 0;
  uint32_t sdTotalMb = 0;
  uint32_t sdFreeMb = 0;
};

/// Appends to a caller's buffer and remembers whether everything fitted. No heap.
class LineWriter {
 public:
  LineWriter(char* out, const size_t cap) : out_(out), cap_(cap) {
    if (cap_ > 0) out_[0] = '\0';
  }

#if defined(__GNUC__)
  __attribute__((format(printf, 2, 3)))
#endif
  void line(const char* fmt, ...) {
    if (used_ + 1 >= cap_) {
      truncated_ = true;
      return;
    }
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(out_ + used_, cap_ - used_, fmt, args);
    va_end(args);
    if (n < 0) return;
    if (static_cast<size_t>(n) >= cap_ - used_) {
      used_ = cap_ - 1;
      truncated_ = true;
      return;
    }
    used_ += static_cast<size_t>(n);
    if (used_ + 1 < cap_) {
      out_[used_++] = '\n';
      out_[used_] = '\0';
    } else {
      truncated_ = true;
    }
  }

  size_t size() const { return used_; }
  bool truncated() const { return truncated_; }

 private:
  char* out_;
  size_t cap_;
  size_t used_ = 0;
  bool truncated_ = false;
};

inline const char* sourceName(const Source source) {
  switch (source) {
    case Source::Cached:
      return "cached";
    case Source::Built:
      return "built";
    case Source::BuiltLive:
      return "shown while building";
    case Source::Unknown:
      break;
  }
  return "unknown";
}

/// "cached" / "built in 2300 ms, 14 pages" / "shown while building, build 5200 ms, 31 pages" /
/// "shown while building, build not finished". English on purpose: it is compared across reports.
inline void describeSection(const SectionTiming& s, char* out, const size_t cap) {
  if (s.source == Source::Cached || s.source == Source::Unknown) {
    snprintf(out, cap, "%s", sourceName(s.source));
  } else if (!s.buildDone) {
    snprintf(out, cap, "%s, build not finished", sourceName(s.source));
  } else if (s.source == Source::Built) {
    snprintf(out, cap, "built in %lu ms, %u pages", static_cast<unsigned long>(s.buildMs), s.pages);
  } else {
    snprintf(out, cap, "%s, build %lu ms, %u pages", sourceName(s.source), static_cast<unsigned long>(s.buildMs),
             s.pages);
  }
}

/// The whole report as plain text. Returns the number of bytes written (without the terminator);
/// a buffer that is too small gets as many whole lines as fit.
inline size_t formatReport(const Log& log, const ReportEnv& env, char* out, const size_t cap) {
  LineWriter w(out, cap);
  char section[72];
  w.line("x4-orbit speed report");
  w.line("firmware: %s", env.firmware);
  w.line("board: %s", env.board);
  w.line("uptime: %lu s", static_cast<unsigned long>(env.uptimeSeconds));
  w.line("settings: %s", env.settings);
  w.line("%s", "");

  w.line("BOOK OPEN (last)");
  const OpenRecord& open = log.lastOpen();
  if (!open.valid) {
    w.line("  none measured since the reader was switched on");
  } else {
    describeSection(open.section, section, sizeof(section));
    w.line("  total: %lu ms (open request to first page settled)", static_cast<unsigned long>(open.totalMs));
    w.line("  load book: %lu ms", static_cast<unsigned long>(open.loadMs));
    w.line("  reader setup: %lu ms", static_cast<unsigned long>(open.setupMs));
    w.line("  chapter: %lu ms (%s)", static_cast<unsigned long>(open.sectionMs), section);
    if (open.section.source == Source::BuiltLive) {
      w.line("  first page draw: included in chapter");
    } else {
      w.line("  first page draw: %lu ms", static_cast<unsigned long>(open.drawMs));
    }
    if (open.fromWake) {
      w.line("  after sleep: yes, %lu ms from power-on to the page", static_cast<unsigned long>(open.sinceBootMs));
    } else {
      w.line("  after sleep: no");
    }
  }
  w.line("%s", "");

  w.line("CHAPTER CHANGE (last)");
  const ChapterRecord& chapter = log.lastChapter();
  if (!chapter.valid) {
    w.line("  none measured since the reader was switched on");
  } else {
    describeSection(chapter.section, section, sizeof(section));
    w.line("  to first page: %lu ms, spine %d, %s", static_cast<unsigned long>(chapter.totalMs), chapter.section.spine,
           section);
  }
  w.line("%s", "");

  w.line("PAGE TURNS (last %u)", log.turnCount());
  if (log.turnCount() == 0) {
    w.line("  none measured since the reader was switched on");
  } else {
    static constexpr struct {
      TurnField field;
      const char* label;
    } kRows[] = {
        {TurnField::Total, "  button to page settled"}, {TurnField::ToRefresh, "  button to refresh start"},
        {TurnField::Wait, "    waiting to be noticed"}, {TurnField::Render, "    drawing the page"},
        {TurnField::Refresh, "  screen refresh"},
    };
    for (const auto& row : kRows) {
      const Stats s = log.turnStats(row.field);
      w.line("%s: median %u, min %u, max %u ms", row.label, s.median, s.min, s.max);
    }
    w.line("  pages drawn ahead of the turn: %u of %u", log.preDrawnTurns(), log.turnCount());
    w.line("  each turn, oldest first (wait/draw/refresh ms, * = drawn ahead):");
    char row[96];
    size_t used = 0;
    for (uint8_t i = 0; i < log.turnCount(); i++) {
      const Turn& t = log.turn(i);
      const int n = snprintf(row + used, sizeof(row) - used, " %u/%u/%u%s", t.waitMs, t.renderMs, t.refreshMs,
                             t.preDrawn ? "*" : "");
      if (n > 0) used += static_cast<size_t>(n);
      if ((i % 4) == 3 || i + 1 == log.turnCount()) {
        w.line("   %s", row);
        used = 0;
      }
    }
  }
  w.line("%s", "");

  w.line("CPU AND POWER");
  w.line("  cpu now: %lu MHz (idle: %lu MHz after %lu ms, light sleep after %lu ms in %lu ms slices)",
         static_cast<unsigned long>(env.cpuMhz), static_cast<unsigned long>(env.cpuIdleMhz),
         static_cast<unsigned long>(env.idleDownclockMs), static_cast<unsigned long>(env.lightSleepAfterMs),
         static_cast<unsigned long>(env.lightSleepSliceMs));
  w.line("  light sleep: %lu s in %lu slices, %lu attempts declined", static_cast<unsigned long>(env.lightSleepSeconds),
         static_cast<unsigned long>(env.lightSleepSlices), static_cast<unsigned long>(env.lightSleepDeclined));
  w.line("  usb power: %s, battery %u%%, free memory %lu bytes", env.usbPower ? "yes" : "no", env.batteryPercent,
         static_cast<unsigned long>(env.freeHeap));
  w.line("%s", "");

  w.line("SD CARD");
  w.line("  size %lu MB, free %lu MB", static_cast<unsigned long>(env.sdTotalMb),
         static_cast<unsigned long>(env.sdFreeMb));
  return w.size();
}

}  // namespace SpeedCheck
