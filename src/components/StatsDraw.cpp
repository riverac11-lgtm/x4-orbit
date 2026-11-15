#include "StatsDraw.h"

#include <GfxRenderer.h>

#include <cmath>
#include <cstdio>

#include "fontIds.h"
#include "util/ChapterMarks.h"
#include "util/ReadingCalendar.h"

namespace StatsDraw {

namespace {

void centred(const GfxRenderer& r, const int font, const int cx, const int y, const char* text,
             const EpdFontFamily::Style style = EpdFontFamily::REGULAR, const bool black = true) {
  r.drawText(font, cx - r.getTextWidth(font, text, style) / 2, y, text, black, style);
}

// A thin chevron, `size` px tall, centred on (cx, cy).
void chevron(const GfxRenderer& r, const int cx, const int cy, const int size, const bool right) {
  const int half = size / 2;
  const int dx = right ? -half / 2 : half / 2;
  for (int t = 0; t < 2; ++t) {
    r.drawLine(cx + dx + t, cy - half, cx - dx + t, cy, true);
    r.drawLine(cx - dx + t, cy, cx + dx + t, cy + half, true);
  }
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

int drawToday(const GfxRenderer& r, const int x, const int y, const int width, const Today& today) {
  constexpr int kRadius = 56;
  constexpr int kThickness = 10;
  const int cx = x + kRadius + 4;
  const int cy = y + kRadius + 2;
  int permille = 1000;
  if (today.goalMinutes > 0) {
    permille = today.minutes >= today.goalMinutes ? 1000 : today.minutes * 1000 / today.goalMinutes;
  }
  if (today.goalMinutes > 0) {
    drawRing(r, cx, cy, kRadius, kThickness, permille);
  } else {
    drawRing(r, cx, cy, kRadius, 2, 1000);  // no goal: a plain circle round the figure
  }

  const int bigH = r.getLineHeight(UI_12_FONT_ID);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  char number[8];
  snprintf(number, sizeof(number), "%d", today.minutes);
  const int blockTop = cy - (bigH + smallH) / 2;
  centred(r, UI_12_FONT_ID, cx, blockTop, number, EpdFontFamily::BOLD);
  centred(r, SMALL_FONT_ID, cx, blockTop + bigH, today.ringCaption);

  const int tx = cx + kRadius + 24;
  const int tw = x + width - tx;
  const int bodyH = r.getLineHeight(UI_10_FONT_ID);
  int ty = cy - (bodyH * 3 + 10) / 2;
  r.drawText(UI_10_FONT_ID, tx, ty, r.truncatedText(UI_10_FONT_ID, today.heading, tw, EpdFontFamily::BOLD).c_str(),
             true, EpdFontFamily::BOLD);
  ty += bodyH;
  r.drawText(UI_10_FONT_ID, tx, ty, r.truncatedText(UI_10_FONT_ID, today.line, tw).c_str());
  ty += bodyH + 10;
  if (today.streak && *today.streak) {
    r.drawText(UI_10_FONT_ID, tx, ty, r.truncatedText(UI_10_FONT_ID, today.streak, tw, EpdFontFamily::BOLD).c_str(),
               true, EpdFontFamily::BOLD);
  }
  return kRadius * 2 + 6;
}

int drawMonth(const GfxRenderer& r, const int x, const int y, const int width, const Month& month) {
  const int titleH = r.getLineHeight(UI_10_FONT_ID);
  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  const int cx = x + width / 2;
  centred(r, UI_10_FONT_ID, cx, y, month.title, EpdFontFamily::BOLD);
  const int titleW = r.getTextWidth(UI_10_FONT_ID, month.title, EpdFontFamily::BOLD);
  if (month.canGoBack) chevron(r, cx - titleW / 2 - 22, y + titleH / 2, 12, false);
  if (month.canGoForward) chevron(r, cx + titleW / 2 + 22, y + titleH / 2, 12, true);

  const int colW = width / 7;
  const int left = x + (width - colW * 7) / 2;
  int cy = y + titleH + 6;

  char letter[12];
  for (int col = 0; col < 7 && ReadingCalendar::listItem(month.weekdays, col, letter, sizeof(letter)); ++col) {
    centred(r, SMALL_FONT_ID, left + col * colW + colW / 2, cy, letter);
  }
  cy += smallH + 4;

  const int cellW = colW - 10 < 40 ? colW - 10 : 40;
  const int cellH = 30;
  constexpr int kRowStep = 36;
  int row = 0;
  for (int day = 1; day <= month.dayCount; ++day) {
    const int col = (month.firstWeekday + day - 1) % 7;
    row = (month.firstWeekday + day - 1) / 7;
    const int bx = left + col * colW + (colW - cellW) / 2;
    const int by = cy + row * kRowStep;
    const uint8_t shade = month.shade[day - 1];
    bool whiteText = false;
    if (shade == 1) {
      r.fillRectDither(bx, by, cellW, cellH, Color::LightGray);
    } else if (shade == 2) {
      r.fillRectDither(bx, by, cellW, cellH, Color::DarkGray);
      whiteText = true;
    } else if (shade >= 3) {
      r.fillRect(bx, by, cellW, cellH, true);
      whiteText = true;
    }
    if (day <= month.lastDay) r.drawRect(bx, by, cellW, cellH, true);
    if (day == month.today) {
      r.drawRect(bx - 3, by - 3, cellW + 6, cellH + 6, true);
      r.drawRect(bx - 2, by - 2, cellW + 4, cellH + 4, true);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", day);
    const int lw = r.getTextWidth(SMALL_FONT_ID, label, EpdFontFamily::BOLD);
    const int ly = by + (cellH - smallH) / 2;
    if (whiteText && shade == 2) {
      // Text on the 50% dither would break up: a small white plate keeps the number readable.
      r.fillRect(bx + (cellW - lw) / 2 - 3, ly + 1, lw + 6, smallH - 2, false);
      whiteText = false;
    }
    r.drawText(SMALL_FONT_ID, bx + (cellW - lw) / 2, ly, label, !whiteText,
               whiteText || day == month.today ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }
  return cy - y + (row + 1) * kRowStep;
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
