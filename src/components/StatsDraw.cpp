#include "StatsDraw.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fontIds.h"
#include "util/ChapterMarks.h"

namespace StatsDraw {

namespace {

void centred(const GfxRenderer& r, const int font, const int cx, const int y, const char* text,
             const EpdFontFamily::Style style = EpdFontFamily::REGULAR, const bool black = true) {
  r.drawText(font, cx - r.getTextWidth(font, text, style) / 2, y, text, black, style);
}

}  // namespace

void drawRing(const GfxRenderer& r, const int cx, const int cy, const int radius, const int thickness, int permille) {
  if (permille < 0) permille = 0;
  const int outer2 = radius * radius;
  const int inner = radius - thickness;
  const int inner2 = inner * inner;
  // The sweep's end as a vector (screen y down), and the cross products that place a pixel before
  // or after it, clockwise from 12 o'clock: one float sin/cos per ring, integers per pixel.
  const float angle = static_cast<float>(permille) * 6.2831853f / 1000.0f;
  const int ex = static_cast<int>(std::lround(std::sin(angle) * 1024.0f));
  const int ey = static_cast<int>(std::lround(-std::cos(angle) * 1024.0f));
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      const int d2 = dx * dx + dy * dy;
      if (d2 > outer2 || d2 < inner2) continue;
      bool on;
      if (permille >= 1000) {
        on = true;
      } else {
        // cross(up, p) with up = (0, -1) is dx; cross(p, end) = dx*ey - dy*ex.
        const long afterStart = dx;
        const long beforeEnd = static_cast<long>(dx) * ey - static_cast<long>(dy) * ex;
        if (permille <= 500) {
          on = (afterStart > 0 || (afterStart == 0 && dy < 0)) && beforeEnd >= 0;
        } else {
          on = !(afterStart < 0 && beforeEnd < 0);
        }
      }
      if (on) {
        r.drawPixel(cx + dx, cy + dy, true);
      } else if (((cx + dx) & 1) == 0 && ((cy + dy) & 1) == 0) {
        r.drawPixel(cx + dx, cy + dy, true);  // the light grey of the remainder, as fillRectDither's
      }
    }
  }
}

int drawBook(const GfxRenderer& r, const int x, const int y, const int width, const Book& book) {
  constexpr int kRadius = 50;
  constexpr int kThickness = 9;
  const int cx = x + kRadius + 4;
  const int ringH = kRadius * 2 + 6;
  const int bodyH = r.getLineHeight(UI_10_FONT_ID);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);

  // The text beside the ring, measured first so the two centre on each other.
  const int tx = cx + kRadius + 22;
  const int tw = x + width - tx;
  const auto title = r.wrappedText(UI_10_FONT_ID, book.title, tw, 2, EpdFontFamily::BOLD);
  const bool hasLine = book.line && *book.line;
  const bool hasNote = book.note && *book.note;
  const bool hasLast = book.last && *book.last;
  const int textH = static_cast<int>(title.size()) * bodyH + (hasLine ? bodyH + 6 : 0) + (hasNote ? smallH : 0) +
                    (hasLast ? smallH + 6 : 0);
  const int height = std::max(ringH, textH);

  const int cy = y + height / 2;
  drawRing(r, cx, cy, kRadius, kThickness, std::clamp(book.percent, 0, 100) * 10);
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", std::clamp(book.percent, 0, 100));
  // "100%" in the large face would touch the ring.
  const int pctFont = book.percent >= 100 ? UI_10_FONT_ID : UI_12_FONT_ID;
  centred(r, pctFont, cx, cy - r.getLineHeight(pctFont) / 2, pct, EpdFontFamily::BOLD);

  int ty = y + (height - textH) / 2;
  for (const auto& line : title) {
    r.drawText(UI_10_FONT_ID, tx, ty, line.c_str(), true, EpdFontFamily::BOLD);
    ty += bodyH;
  }
  if (hasLine) {
    ty += 6;
    r.drawText(UI_10_FONT_ID, tx, ty, r.truncatedText(UI_10_FONT_ID, book.line, tw).c_str());
    ty += bodyH;
  }
  if (hasNote) {
    r.drawText(SMALL_FONT_ID, tx, ty, r.truncatedText(SMALL_FONT_ID, book.note, tw).c_str());
    ty += smallH;
  }
  if (hasLast) {
    ty += 6;
    r.drawText(SMALL_FONT_ID, tx, ty, r.truncatedText(SMALL_FONT_ID, book.last, tw).c_str());
  }
  return height;
}

int drawSessionBars(const GfxRenderer& r, const int x, const int y, const int width, const int height,
                    const uint32_t* seconds, const int count, const int capacity) {
  if (capacity <= 0) return 0;
  uint32_t longest = 1;
  for (int i = 0; i < count; ++i) longest = std::max(longest, seconds[i]);
  const int gap = std::max(3, width / (capacity * 5));
  const int barW = std::max(2, (width - (capacity - 1) * gap) / capacity);
  const int span = capacity * barW + (capacity - 1) * gap;
  const int left = x + (width - span) / 2;
  const int base = y + height;
  r.drawLine(left, base, left + span - 1, base, true);
  // The newest session takes the right-hand slot; the empty slots wait on the left.
  const int empty = capacity - std::min(count, capacity);
  for (int slot = 0; slot < capacity; ++slot) {
    const int bx = left + slot * (barW + gap);
    if (slot < empty) {
      r.drawLine(bx + barW / 2, base - 3, bx + barW / 2, base, true);
      continue;
    }
    const uint32_t s = seconds[slot - empty];
    const int h = std::max<int>(3, static_cast<int>(static_cast<uint64_t>(s) * height / longest));
    r.fillRect(bx, base - h, barW, h, true);
  }
  return height + 1;
}

int drawMilestone(const GfxRenderer& r, const int x, const int y, const int width, const Milestone& milestone) {
  const int bodyH = r.getLineHeight(UI_10_FONT_ID);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  const int valueW = r.getTextWidth(UI_10_FONT_ID, milestone.value, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, x + width - valueW, y, milestone.value, true, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, x, y,
             r.truncatedText(UI_10_FONT_ID, milestone.label, width - valueW - 12, EpdFontFamily::BOLD).c_str(), true,
             EpdFontFamily::BOLD);
  constexpr int kBarH = 6;
  const int barY = y + bodyH + 2;
  r.drawRect(x, barY, width, kBarH, true);
  const int filled = static_cast<int>(static_cast<int64_t>(width) * std::clamp(milestone.permille, 0, 1000) / 1000);
  if (filled > 0) r.fillRect(x, barY, filled, kBarH, true);
  const int ny = barY + kBarH + 2;
  r.drawText(SMALL_FONT_ID, x, ny, r.truncatedText(SMALL_FONT_ID, milestone.next, width).c_str());
  return ny + smallH - y;
}

void drawQuietText(const GfxRenderer& r, const int right, const int y, const char* text) {
  const int w = r.getTextWidth(SMALL_FONT_ID, text);
  const int x = right - w;
  r.drawText(SMALL_FONT_ID, x, y, text);
  const int h = r.getLineHeight(SMALL_FONT_ID);
  for (int py = y; py < y + h; ++py) {
    for (int px = x + (py & 1); px < x + w + 1; px += 2) r.drawPixel(px, py, false);
  }
}

void drawChapterTicks(const GfxRenderer& r, const int x, const int y, const int width, const int height,
                      const int filled, const bool fromRight, const bool upward, const int rise,
                      const ChapterMarks& marks) {
  int16_t offsets[ChapterMarks::kMax];
  const int n = marks.tickOffsets(width, 4, offsets);
  for (int i = 0; i < n; ++i) {
    const int off = fromRight ? width - 1 - offsets[i] : offsets[i];
    const int tx = x + off;
    const bool read = fromRight ? off >= width - filled : off < filled;
    if (read) {
      r.drawLine(tx, y, tx, y + height - 1, false);  // a notch through the read part
    } else {
      r.drawLine(tx, y, tx, y + height - 1, true);
    }
    if (rise > 0) {
      if (upward) {
        r.drawLine(tx, y - rise, tx, y - 1, true);
      } else {
        r.drawLine(tx, y + height, tx, y + height + rise - 1, true);
      }
    }
  }
}

}  // namespace StatsDraw
