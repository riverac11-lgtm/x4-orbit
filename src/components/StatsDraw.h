#pragma once

#include <cstdint>

class GfxRenderer;
struct ChapterMarks;

// The Reading Stats screen's cards, the rings of the Orbit homes' stats graphic, and the chapter
// ticks on the reader's progress bar, painted from plain values. Nothing here reads a file, a
// setting or the clock, so test/orbit_preview renders the very same code on the host.
namespace StatsDraw {

// A ring `thickness` px wide, filled clockwise from 12 o'clock for `permille` of a turn, the rest
// light grey.
void drawRing(const GfxRenderer& r, int cx, int cy, int radius, int thickness, int permille);

// The current book: its progress as a ring with the percent in it, beside it the title (two lines
// at most) and up to three lines under it. Every string is the caller's, translated and formatted;
// an empty one is left out.
struct Book {
  int percent = 0;
  const char* title = "";
  const char* line = "";  // "About 3 h 10 min left"
  const char* note = "";  // "~8 sessions at your pace" (small)
  const char* last = "";  // "Last session: 42 min, 38 pages" (small)
};
// Draws into the box at (x, y), `width` wide; returns the height used.
int drawBook(const GfxRenderer& r, int x, int y, int width, const Book& book);

// The recent sessions as bars `height` px tall, oldest left, newest right, scaled to the longest;
// slots not filled yet are a short stroke on the baseline. Returns the height used.
int drawSessionBars(const GfxRenderer& r, int x, int y, int width, int height, const uint32_t* seconds, int count,
                    int capacity);

// One milestone: the label and the figure on a line, then a bar of the way to the next threshold
// and the small "Next: 50 hours · 13 h to go" under it. Returns the height used.
struct Milestone {
  const char* label = "";  // "Hours read"
  const char* value = "";  // "37"
  const char* next = "";   // "Next: 50 hours · 13 h to go"
  int permille = 0;
};
int drawMilestone(const GfxRenderer& r, int x, int y, int width, const Milestone& milestone);

// Small text ending at `right`, in a light grey (every other pixel of it cleared): the quiet
// "~9 days" beside the battery on the Orbit home screens.
void drawQuietText(const GfxRenderer& r, int right, int y, const char* text);

// The ticks over a progress bar at (x, y, width, height) whose read part is `filled` px from the
// left (from the right when `fromRight`): a short black tick standing `rise` px off the bar on the
// side away from the screen edge (`upward`), and a white notch through the read part.
void drawChapterTicks(const GfxRenderer& r, int x, int y, int width, int height, int filled, bool fromRight,
                      bool upward, int rise, const ChapterMarks& marks);

}  // namespace StatsDraw
