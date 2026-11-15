#pragma once

#include <cstdint>

class GfxRenderer;
struct ChapterMarks;

// The Reading Stats goal ring and month calendar, and the chapter ticks on the reader's progress
// bar, painted from plain values. Nothing here reads a file, a setting or the clock, so
// test/orbit_preview renders the very same code on the host.
namespace StatsDraw {

// The "today" block: a ring filling toward the goal with the minutes in it, and beside it the day's
// line and the streak. Every string is the caller's, already translated and formatted.
struct Today {
  int minutes = 0;
  int goalMinutes = 0;           // 0: no goal; the ring is then drawn full and light
  const char* ringCaption = "";  // "of 30 min" / "min"
  const char* heading = "";      // "Today"
  const char* line = "";         // "12 min to your goal"
  const char* streak = "";       // "6-day streak" (empty: none)
};
// Draws into the box at (x, y), `width` wide; returns the height used.
int drawToday(const GfxRenderer& r, int x, int y, int width, const Today& today);

// A ring `thickness` px wide, filled clockwise from 12 o'clock for `permille` of a turn, the rest
// light grey.
void drawRing(const GfxRenderer& r, int cx, int cy, int radius, int thickness, int permille);

struct Month {
  const char* title = "";     // "November 2026"
  const char* weekdays = "";  // seven letters, '|'-separated, Monday first: "M|T|W|T|F|S|S"
  int firstWeekday = 0;       // 0 = Monday
  int dayCount = 30;
  uint8_t shade[31] = {};  // 0 (none) .. 3 (goal met), ReadingCalendar::shade()
  int today = 0;           // day of the month to ring, 0: today is not in this month
  int lastDay = 31;        // days after this are still to come, drawn without a box
  bool canGoBack = false;  // chevrons beside the title
  bool canGoForward = false;
};
// Returns the height used.
int drawMonth(const GfxRenderer& r, int x, int y, int width, const Month& month);

// Small text ending at `right`, in a light grey (every other pixel of it cleared): the quiet
// "~9 days" beside the battery on the Orbit home screens.
void drawQuietText(const GfxRenderer& r, int right, int y, const char* text);

// The ticks over a progress bar at (x, y, width, height) whose read part is `filled` px from the
// left (from the right when `fromRight`): a short black tick standing `rise` px off the bar on the
// side away from the screen edge (`upward`), and a white notch through the read part.
void drawChapterTicks(const GfxRenderer& r, int x, int y, int width, int height, int filled, bool fromRight,
                      bool upward, int rise, const ChapterMarks& marks);

}  // namespace StatsDraw
