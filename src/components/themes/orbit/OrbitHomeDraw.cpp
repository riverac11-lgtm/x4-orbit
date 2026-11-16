#include "OrbitHomeDraw.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "components/StatsDraw.h"
#include "fontIds.h"

namespace OrbitHomeDraw {

using OrbitHomeLayout::Box;
using OrbitHomeLayout::Frame;
using OrbitHomeLayout::Variant;

namespace {

constexpr int kTileRadius = 14;
constexpr int kRowRadius = 10;

// ---- Primitive shapes --------------------------------------------------------------------------

// Pixels within `r` of (cx, cy) painted `state`; a ring of `width` px when width > 0. Integer
// arithmetic on squared distances, so a 48 px icon costs a few thousand comparisons and no table.
void disc(const GfxRenderer& r, const int cx, const int cy, const int radius, const bool state, const int width = 0) {
  if (radius <= 0) return;
  const int outer = radius * radius + radius;  // +r rounds the edge like a half-pixel margin
  const int innerR = width > 0 ? radius - width : -1;
  const int inner = innerR >= 0 ? innerR * innerR + innerR : -1;
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      const int d = dx * dx + dy * dy;
      if (d <= outer && d > inner) r.drawPixel(cx + dx, cy + dy, state);
    }
  }
}

void centredText(const GfxRenderer& r, const int fontId, const Box& box, const int y, const char* text,
                 const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const std::string fitted = r.truncatedText(fontId, text, box.w, style);
  const int w = r.getTextWidth(fontId, fitted.c_str(), style);
  r.drawText(fontId, box.x + (box.w - w) / 2, y, fitted.c_str(), true, style);
}

void leftText(const GfxRenderer& r, const int fontId, const int x, const int y, const int maxW, const std::string& text,
              const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (text.empty() || maxW <= 0) return;
  r.drawText(fontId, x, y, r.truncatedText(fontId, text.c_str(), maxW, style).c_str(), true, style);
}

// A line `width` px thick, thickened across its run: GfxRenderer's own thick line always grows
// downward, which leaves a steep stroke (a chevron arm, a leaning book) one pixel wide.
void thickLine(const GfxRenderer& r, const int x1, const int y1, const int x2, const int y2, const int width) {
  const bool steep = std::abs(y2 - y1) > std::abs(x2 - x1);
  for (int i = 0; i < width; ++i) {
    const int off = i - (width - 1) / 2;
    if (steep) {
      r.drawLine(x1 + off, y1, x2 + off, y2, true);
    } else {
      r.drawLine(x1, y1 + off, x2, y2 + off, true);
    }
  }
}

// A dotted hairline: grey on a 1-bit panel without the weight of a solid rule.
void dottedLine(const GfxRenderer& r, const int x, const int y, const int w) {
  for (int i = 0; i < w; i += 3) r.drawPixel(x + i, y, true);
}

void drawIcon(const GfxRenderer& r, const View& v, const int i, const int cx, const int cy, const int size) {
  if (drawLineIcon(r, v.menuIcon[i], cx, cy, size)) return;
  if (v.menuBitmap[i] != nullptr) r.drawIcon(v.menuBitmap[i], cx - 16, cy - 16, 32, 32);
}

std::string etaLine(const View& v) {
  if (v.book.eta.empty()) return {};
  char buf[48];
  snprintf(buf, sizeof(buf), v.leftFormat, v.book.eta.c_str());
  return buf;
}

std::string percentText(const int percent) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", std::clamp(percent, 0, 100));
  return buf;
}

void cover(GfxRenderer& r, const View& v, const int index, const Box& slot) {
  if (v.drawCover != nullptr) v.drawCover(v.coverCtx, r, index, slot);
}

// ---- The Reading Stats graphic -----------------------------------------------------------------

int statsPermille(const View::Stats& s) {
  if (s.goalMinutes <= 0 || s.minutes <= 0) return 0;
  return std::min(1000, s.minutes * 1000 / s.goalMinutes);
}

// The day's ring on a white disc (it stays readable on a selected tile's grey), the minutes in it when
// there is room.
void statsRing(const GfxRenderer& r, const View::Stats& s, const int cx, const int cy, const int radius,
               const int fontId, const bool number) {
  disc(r, cx, cy, radius + 2, false);
  StatsDraw::drawRing(r, cx, cy, radius, std::max(3, radius / 5), statsPermille(s));
  if (!number) return;
  char minutes[8];
  snprintf(minutes, sizeof(minutes), "%d", std::clamp(s.minutes, 0, 999));
  const int w = r.getTextWidth(fontId, minutes, EpdFontFamily::BOLD);
  r.drawText(fontId, cx - w / 2, cy - r.getLineHeight(fontId) / 2, minutes, true, EpdFontFamily::BOLD);
}

// The last seven days as bars, oldest first, scaled to the larger of the goal and the week's best day,
// with the goal as a dotted line; a day without reading is a short stroke on the baseline.
void weekBars(const GfxRenderer& r, const View::Stats& s, const Box& b) {
  if (b.empty()) return;
  int top = s.goalMinutes;
  for (const uint16_t m : s.week) top = std::max<int>(top, m);
  if (top <= 0) top = 1;
  const int gap = std::max(2, b.w / 20);
  const int barW = std::max(2, (b.w - 6 * gap) / 7);
  for (int i = 0; i < 7; ++i) {
    const int x = b.x + i * (barW + gap);
    const int h = s.week[i] == 0 ? 1 : std::max(3, b.h * std::min<int>(s.week[i], top) / top);
    r.fillRect(x, b.y + b.h - h, barW, h, true);
  }
  if (s.goalMinutes > 0) {
    const int gy = b.y + b.h - b.h * s.goalMinutes / top;
    for (int x = b.x; x < b.x + 7 * (barW + gap) - gap; x += 3) r.drawPixel(x, gy, true);
  }
}

bool isStats(const View& v, const int i) { return v.menuIcon[i] == UIIcon::Stats; }

// A's tile: the ring with the minutes in it, "Today / of 30 min / 6-day streak" beside it, the week's
// bars across the bottom. A short tile (many entries) keeps the ring and one line.
void statsTile(const GfxRenderer& r, const View& v, const Box& t, const bool selected,
               const OrbitHomeLayout::Fonts& fonts) {
  const View::Stats& s = v.stats;
  const auto style = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  if (!s.hasHistory) {
    const int iconY = t.h >= 100 ? t.y + t.h * 2 / 5 : t.y + t.h / 2;
    if (t.h >= 100) {
      drawLineIcon(r, UIIcon::Stats, t.x + t.w / 2, t.y + t.h / 3, 40);
      const auto lines = r.wrappedText(SMALL_FONT_ID, s.empty, t.w - 24, 2);
      int y = t.y + t.h * 3 / 5;
      for (const auto& line : lines) {
        centredText(r, SMALL_FONT_ID, Box{t.x + 12, 0, t.w - 24, 0}, y, line.c_str());
        y += fonts.small;
      }
    } else {
      drawLineIcon(r, UIIcon::Stats, t.x + 20 + 16, iconY, 32);
      leftText(r, SMALL_FONT_ID, t.x + 64, t.y + (t.h - fonts.small) / 2, t.w - 72, s.empty);
    }
    return;
  }
  if (t.h < 100) {
    const int radius = std::min(18, t.h / 2 - 6);
    statsRing(r, s, t.x + 16 + radius, t.y + t.h / 2, radius, UI_10_FONT_ID, false);
    const int tx = t.x + 32 + 2 * radius;
    leftText(r, UI_10_FONT_ID, tx, t.y + (t.h - fonts.body) / 2, t.x + t.w - 8 - tx, s.compact, style);
    return;
  }
  const int barsH = std::clamp(t.h / 6, 14, 26);
  const Box bars{t.x + 18, t.y + t.h - 16 - barsH, t.w - 36, barsH};
  const int ringTop = t.y + 14;
  const int radius = std::clamp((bars.y - 12 - ringTop) / 2, 16, 34);
  const int cx = t.x + 18 + radius;
  const int cy = ringTop + radius;
  statsRing(r, s, cx, cy, radius, radius >= 26 ? UI_12_FONT_ID : UI_10_FONT_ID, true);
  const int tx = cx + radius + 12;
  const int tw = t.x + t.w - 10 - tx;
  int y = cy - (fonts.small + fonts.body + fonts.small + 4) / 2;
  leftText(r, SMALL_FONT_ID, tx, y, tw, s.heading);
  y += fonts.small + 2;
  leftText(r, UI_10_FONT_ID, tx, y, tw, s.caption, style);
  y += fonts.body + 2;
  leftText(r, SMALL_FONT_ID, tx, y, tw, s.streak);
  weekBars(r, s, bars);
}

// C's row: the ring, the day and the streak on two lines, the week's bars before the chevron.
void statsRow(const GfxRenderer& r, const View& v, const Box& row, const bool selected,
              const OrbitHomeLayout::Fonts& fonts) {
  const View::Stats& s = v.stats;
  const int cy = row.y + row.h / 2;
  const int tx = row.x + 72;
  const auto style = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  if (!s.hasHistory) {
    drawLineIcon(r, UIIcon::Stats, row.x + 36, cy, 32);
    leftText(r, UI_10_FONT_ID, tx, cy - fonts.body / 2, row.x + row.w - 40 - tx, s.empty, style);
    return;
  }
  const int radius = std::min(22, row.h / 2 - 3);
  statsRing(r, s, row.x + 36, cy, radius, UI_10_FONT_ID, radius >= 18);
  const int barsW = 62;
  const int barsH = std::min(28, row.h - 20);
  const Box bars{row.x + row.w - 40 - barsW, cy - barsH / 2, barsW, barsH};
  const int tw = bars.x - 12 - tx;
  const bool twoLines = row.h >= fonts.body + fonts.small + 6;
  if (twoLines) {
    const int y = cy - (fonts.body + fonts.small + 2) / 2;
    leftText(r, UI_10_FONT_ID, tx, y, tw, s.today, style);
    leftText(r, SMALL_FONT_ID, tx, y + fonts.body + 2, tw, s.streak);
  } else {
    leftText(r, UI_10_FONT_ID, tx, cy - fonts.body / 2, tw, s.today, style);
  }
  weekBars(r, s, bars);
}

// ---- A: card + tiles ---------------------------------------------------------------------------

void drawA(GfxRenderer& r, const View& v, const Frame& f, const OrbitHomeLayout::Fonts& fonts) {
  const int bookCount = v.hasBook ? 1 : 0;
  leftText(r, SMALL_FONT_ID, f.caption.x, f.caption.y, f.caption.w, v.caption);

  const bool cardSelected = v.hasBook && v.selected == 0;
  if (cardSelected) r.fillRoundedRect(f.book.x, f.book.y, f.book.w, f.book.h, kTileRadius, Color::LightGray);
  r.drawRoundedRect(f.book.x, f.book.y, f.book.w, f.book.h, cardSelected ? 3 : 2, kTileRadius, true);

  if (v.hasBook) {
    cover(r, v, 0, f.cover);
    int y = f.text.y;
    const auto title = r.wrappedText(UI_12_FONT_ID, v.book.title.c_str(), f.text.w, 2, EpdFontFamily::BOLD);
    for (const auto& line : title) {
      r.drawText(UI_12_FONT_ID, f.text.x, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += fonts.title;
    }
    y += 4;
    if (!v.book.author.empty()) {
      leftText(r, UI_10_FONT_ID, f.text.x, y, f.text.w, v.book.author);
      y += fonts.body + 2;
    }
    if (!v.book.series.empty() && y + fonts.small <= f.status.y) {
      leftText(r, SMALL_FONT_ID, f.text.x, y, f.text.w, v.book.series);
    }
    if (v.book.percent >= 0) {
      const std::string pct = percentText(v.book.percent);
      r.drawText(UI_12_FONT_ID, f.status.x, f.status.y, pct.c_str(), true, EpdFontFamily::BOLD);
      const int pw = r.getTextWidth(UI_12_FONT_ID, pct.c_str(), EpdFontFamily::BOLD);
      leftText(r, SMALL_FONT_ID, f.status.x + pw + 8, f.status.y + fonts.title - fonts.small - 2, f.status.w - pw - 8,
               etaLine(v));
      drawProgressBar(r, f.bar, v.book.percent);
    }
    leftText(r, SMALL_FONT_ID, f.history.x, f.history.y, f.history.w, v.book.history);
  } else {
    r.drawText(UI_12_FONT_ID, f.text.x, f.text.y, v.noBookTitle, true, EpdFontFamily::BOLD);
    leftText(r, UI_10_FONT_ID, f.text.x, f.text.y + fonts.title + 4, f.text.w, v.noBookHint);
  }

  for (int i = 0; i < f.menuCount; ++i) {
    const Box& t = f.menu[i];
    const bool selected = v.selected == bookCount + i;
    if (selected) r.fillRoundedRect(t.x, t.y, t.w, t.h, kTileRadius, Color::LightGray);
    r.drawRoundedRect(t.x, t.y, t.w, t.h, selected ? 3 : 2, kTileRadius, true);
    if (isStats(v, i)) {
      statsTile(r, v, t, selected, fonts);
      continue;
    }
    const auto style = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    if (t.h >= 100) {
      drawIcon(r, v, i, t.x + t.w / 2, t.y + t.h * 2 / 5, f.iconSize);
      centredText(r, UI_10_FONT_ID, Box{t.x + 8, 0, t.w - 16, 0}, t.y + t.h * 7 / 10 - fonts.body / 2, v.menuLabel[i],
                  style);
    } else {
      // A short tile (many entries): icon and label side by side.
      drawIcon(r, v, i, t.x + 20 + f.iconSize / 2, t.y + t.h / 2, f.iconSize);
      const int tx = t.x + 32 + f.iconSize;
      leftText(r, UI_10_FONT_ID, tx, t.y + (t.h - fonts.body) / 2, t.x + t.w - 8 - tx, v.menuLabel[i], style);
    }
  }
}

// ---- B: cover first ----------------------------------------------------------------------------

void drawB(GfxRenderer& r, const View& v, const Frame& f, const OrbitHomeLayout::Fonts& fonts) {
  const int bookCount = v.hasBook ? 1 : 0;
  if (v.hasBook) {
    cover(r, v, 0, f.cover);
    if (v.selected == 0) r.drawRoundedRect(f.book.x, f.book.y, f.book.w, f.book.h, 3, 8, true);
    int y = f.text.y;
    centredText(r, UI_12_FONT_ID, f.text, y, v.book.title.c_str(), EpdFontFamily::BOLD);
    y += fonts.title + 4;
    std::string byline = v.book.author;
    if (!v.book.series.empty()) byline += byline.empty() ? v.book.series : "  \xC2\xB7  " + v.book.series;
    centredText(r, UI_10_FONT_ID, f.text, y, byline.c_str());
    if (v.book.percent >= 0) {
      drawProgressBar(r, f.bar, v.book.percent);
      const std::string pct = percentText(v.book.percent);
      r.drawText(UI_10_FONT_ID, f.status.x, f.status.y, pct.c_str(), true, EpdFontFamily::BOLD);
      const std::string eta = etaLine(v);
      if (!eta.empty()) {
        const int w = r.getTextWidth(SMALL_FONT_ID, eta.c_str());
        r.drawText(SMALL_FONT_ID, f.status.x + f.status.w - w, f.status.y + 2, eta.c_str());
      }
    }
    // The book's history and the day's reading share the line; the day's alone when both do not fit.
    std::string line = v.book.history;
    if (!v.today.empty()) {
      line = line.empty() ? v.today : line + "  \xC2\xB7  " + v.today;
      if (r.getTextWidth(SMALL_FONT_ID, line.c_str()) > f.history.w) line = v.today;
    }
    if (!line.empty()) centredText(r, SMALL_FONT_ID, f.history, f.history.y, line.c_str());
  } else {
    // An empty cover outline holds the place of the book, so the dock sits where it always does.
    r.drawRoundedRect(f.cover.x, f.cover.y, f.cover.w, f.cover.h, 2, 8, true);
    drawLineIcon(r, UIIcon::Folder, f.cover.x + f.cover.w / 2, f.cover.y + f.cover.h / 2, 56);
    centredText(r, UI_12_FONT_ID, f.text, f.text.y, v.noBookTitle, EpdFontFamily::BOLD);
    centredText(r, UI_10_FONT_ID, f.text, f.text.y + fonts.title + 4, v.noBookHint);
  }
  dottedLine(r, f.rule.x, f.rule.y, f.rule.w);

  for (int i = 0; i < f.menuCount; ++i) {
    const Box& t = f.menu[i];
    const bool selected = v.selected == bookCount + i;
    if (selected) r.fillRoundedRect(t.x, t.y, t.w, t.h, kTileRadius, Color::LightGray);
    // Reading Stats: an icon-size ring of the day over "18/30 min" (with no reading yet, its icon and
    // name as any other entry).
    const bool stats = isStats(v, i) && v.stats.hasHistory;
    if (stats) {
      statsRing(r, v.stats, t.x + t.w / 2, t.y + t.h * 9 / 25, f.iconSize / 2, UI_10_FONT_ID, false);
    } else {
      drawIcon(r, v, i, t.x + t.w / 2, t.y + t.h * 9 / 25, f.iconSize);
    }
    // A dock slot is narrow: a two-word label ("Online Library") takes a second line rather than
    // losing its second word.
    const auto style = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const Box labelBox{t.x - 4, 0, t.w + 8, 0};
    const auto lines = r.wrappedText(UI_10_FONT_ID, stats ? v.stats.compact : v.menuLabel[i], labelBox.w, 2, style);
    int ly = t.y + t.h * 7 / 10 - fonts.body / 2 - (static_cast<int>(lines.size()) - 1) * fonts.body / 2;
    for (const auto& line : lines) {
      centredText(r, UI_10_FONT_ID, labelBox, ly, line.c_str(), style);
      ly += fonts.body;
    }
  }
}

// ---- C: list + recent shelf --------------------------------------------------------------------

void drawC(GfxRenderer& r, const View& v, const Frame& f, const OrbitHomeLayout::Fonts& fonts) {
  const int bookCount = (v.hasBook ? 1 : 0) + f.shelfCount;
  leftText(r, UI_12_FONT_ID, f.caption.x, f.caption.y, f.caption.w, v.caption, EpdFontFamily::BOLD);

  if (v.hasBook) {
    if (v.selected == 0) r.fillRoundedRect(f.book.x, f.book.y, f.book.w, f.book.h, kRowRadius, Color::LightGray);
    cover(r, v, 0, f.cover);
    int y = f.text.y;
    leftText(r, UI_12_FONT_ID, f.text.x, y, f.text.w, v.book.title, EpdFontFamily::BOLD);
    y += fonts.title + 2;
    leftText(r, UI_10_FONT_ID, f.text.x, y, f.text.w, v.book.author);
    y += fonts.body + 2;
    if (y + fonts.small <= f.bar.y) leftText(r, SMALL_FONT_ID, f.text.x, y, f.text.w, v.book.series);
    if (v.book.percent >= 0) {
      drawProgressBar(r, f.bar, v.book.percent);
      std::string status = percentText(v.book.percent);
      const std::string eta = etaLine(v);
      if (!eta.empty()) status += "  \xC2\xB7  " + eta;
      leftText(r, SMALL_FONT_ID, f.status.x, f.status.y, f.status.w, status);
    }
  } else {
    leftText(r, UI_12_FONT_ID, f.text.x, f.text.y, f.text.w, v.noBookTitle, EpdFontFamily::BOLD);
    leftText(r, UI_10_FONT_ID, f.text.x, f.text.y + fonts.title + 4, f.text.w, v.noBookHint);
  }

  if (f.shelfCount > 0) {
    leftText(r, UI_10_FONT_ID, f.shelfLabel.x, f.shelfLabel.y - (fonts.body - fonts.small), f.shelfLabel.w,
             v.shelfLabel, EpdFontFamily::BOLD);
    const int first = v.hasBook ? 1 : 0;
    for (int i = 0; i < f.shelfCount; ++i) {
      const Box& s = f.shelf[i];
      cover(r, v, first + i, s);
      if (v.selected == first + i) r.drawRoundedRect(s.x - 5, s.y - 5, s.w + 10, s.h + 10, 3, 6, true);
    }
  }

  for (int i = 0; i < f.menuCount; ++i) {
    const Box& row = f.menu[i];
    const bool selected = v.selected == bookCount + i;
    if (selected) r.fillRoundedRect(row.x, row.y, row.w, row.h, kRowRadius, Color::LightGray);
    const int cy = row.y + row.h / 2;
    const int tx = row.x + 72;
    if (isStats(v, i)) {
      statsRow(r, v, row, selected, fonts);
    } else {
      drawIcon(r, v, i, row.x + 36, cy, f.iconSize);
      const int fontId = row.h >= 44 ? UI_12_FONT_ID : UI_10_FONT_ID;
      const int lh = row.h >= 44 ? fonts.title : fonts.body;
      leftText(r, fontId, tx, cy - lh / 2, row.x + row.w - 40 - tx, v.menuLabel[i],
               selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }
    drawChevron(r, row.x + row.w - 20, cy, 14, true);
    // A hairline between rows; none against the selection, whose fill already separates it.
    const bool nextSelected = v.selected == bookCount + i + 1;
    if (i + 1 < f.menuCount && !selected && !nextSelected) {
      const int ly = (row.y + row.h + f.menu[i + 1].y) / 2;
      dottedLine(r, tx, ly, row.x + row.w - 8 - tx);
    }
  }
}

}  // namespace

OrbitHomeLayout::Fonts fontsOf(const GfxRenderer& renderer) {
  return OrbitHomeLayout::Fonts{renderer.getLineHeight(UI_12_FONT_ID), renderer.getLineHeight(UI_10_FONT_ID),
                                renderer.getLineHeight(SMALL_FONT_ID)};
}

Frame layout(const GfxRenderer& renderer, const View& view) {
  return OrbitHomeLayout::frameFor(view.variant, view.width, view.top, view.bottom, view.hasBook, view.shelfCount,
                                   view.menuCount, fontsOf(renderer));
}

void draw(GfxRenderer& renderer, const View& view, const Frame& frame) {
  const auto fonts = fontsOf(renderer);
  switch (view.variant) {
    case Variant::A:
      drawA(renderer, view, frame, fonts);
      break;
    case Variant::B:
      drawB(renderer, view, frame, fonts);
      break;
    case Variant::C:
      drawC(renderer, view, frame, fonts);
      break;
  }
}

void drawProgressBar(const GfxRenderer& r, const Box& bar, const int percent) {
  if (bar.empty()) return;
  const int radius = bar.h / 2;
  const int fill = OrbitHomeLayout::barFill(bar.w, bar.h, percent);
  if (fill > 0) r.fillRoundedRect(bar.x, bar.y, fill, bar.h, radius, Color::Black);
  r.drawRoundedRect(bar.x, bar.y, bar.w, bar.h, 2, radius, true);
}

void drawChevron(const GfxRenderer& r, const int cx, const int cy, const int size, const bool right,
                 const int thickness) {
  const int half = size / 2;
  const int depth = std::max(3, size / 2);
  const int tipX = right ? cx + depth / 2 : cx - depth / 2;
  const int backX = right ? cx - depth / 2 : cx + depth / 2;
  thickLine(r, backX, cy - half, tipX, cy, thickness);
  thickLine(r, tipX, cy, backX, cy + half, thickness);
}

bool drawLineIcon(const GfxRenderer& r, const UIIcon icon, const int cx, const int cy, const int size) {
  // Designed on a 36-unit grid (the mock-ups' icons), scaled to `size`.
  const auto k = [size](const int v) { return v * size / 36; };
  const int stroke = size >= 44 ? 3 : 2;
  switch (icon) {
    case UIIcon::Folder: {  // Library: three books on a shelf, the last one leaning
      for (const int dx : {-16, -6, 4}) r.drawRect(cx + k(dx), cy - k(16), k(9) + 1, k(32) + 1, stroke, true);
      thickLine(r, cx + k(15), cy - k(14), cx + k(21), cy + k(16), stroke + 1);
      return true;
    }
    case UIIcon::Library: {  // Online Library: a cloud with an arrow coming down out of it
      struct Lobe {
        int x, y, r;
      };
      const Lobe lobes[] = {{-10, 3, 9}, {3, -4, 11}, {13, 4, 8}};
      for (const auto& l : lobes) disc(r, cx + k(l.x), cy + k(l.y), k(l.r), true);
      for (const auto& l : lobes) disc(r, cx + k(l.x), cy + k(l.y), k(l.r) - stroke, false);
      r.fillRect(cx - k(10), cy + k(1), k(23), k(11) - stroke + 1, false);
      r.fillRect(cx - k(10), cy + k(12) - stroke + 1, k(23), stroke, true);
      // The arrow, on a white gap so it reads across the cloud's bottom edge.
      r.fillRect(cx + k(1) - stroke - 1, cy - k(4), 2 * stroke + 3, k(19), false);
      r.fillRect(cx + k(1) - stroke / 2, cy - k(4), stroke, k(19), true);
      const int ax[] = {cx + k(-6), cx + k(8), cx + k(1)};
      const int ay[] = {cy + k(14), cy + k(14), cy + k(22)};
      r.fillPolygon(ax, ay, 3, true);
      return true;
    }
    case UIIcon::Ellipsis: {
      const int dot = std::max(2, size / 11);
      for (const int dx : {-12, 0, 12}) disc(r, cx + k(dx), cy, dot, true);
      return true;
    }
    case UIIcon::Settings: {  // two sliders
      for (const auto& [dy, px] : {std::pair<int, int>{-8, -6}, {8, 6}}) {
        r.fillRect(cx - k(18), cy + k(dy) - stroke / 2, k(36), stroke, true);
        disc(r, cx + k(px), cy + k(dy), k(5) + 1, true);
        disc(r, cx + k(px), cy + k(dy), k(5) + 1 - stroke, false);
      }
      return true;
    }
    case UIIcon::Stats: {  // three bars
      const int heights[] = {10, 18, 26};
      for (int i = 0; i < 3; ++i) {
        r.fillRect(cx - k(15) + i * k(11), cy + k(13) - k(heights[i]), k(8), k(heights[i]), true);
      }
      return true;
    }
    case UIIcon::Book: {  // Bookmarks: a ribbon
      const int x0 = cx - k(9), x1 = cx + k(9), y0 = cy - k(15), y1 = cy + k(15);
      thickLine(r, x0, y0, x1, y0, stroke);
      thickLine(r, x0, y0, x0, y1, stroke);
      thickLine(r, x1, y0, x1, y1, stroke);
      thickLine(r, x0, y1, cx, cy + k(7), stroke);
      thickLine(r, x1, y1, cx, cy + k(7), stroke);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace OrbitHomeDraw
