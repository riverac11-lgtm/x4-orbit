#pragma once

#include <algorithm>
#include <cstdint>

// Geometry and button navigation of the three Orbit home screens, kept free of the renderer so the
// unit tests (test/orbit_home_layout) and the host preview (test/orbit_preview) run the same
// arithmetic the device does.
//
//   A  "Card + tiles":        a continue-reading card, then the home menu as tiles in two columns.
//   B  "Cover first":         a large centred cover with its progress, then the menu as an icon dock.
//   C  "List + recent shelf": a greeting, a compact current-book row, a shelf of up to four covers,
//                             then the menu as a list.
//
// The selector is the one HomeActivity already keeps: books first (the current book, then the
// shelf), then the menu entries. Each layout arranges those items in rows; Left/Right step through
// them in order and Up/Down move between rows, so a reader with only the front buttons reaches
// everything and the side buttons are a shortcut.
namespace OrbitHomeLayout {

enum class Variant : uint8_t { A = 1, B = 2, C = 3 };

inline constexpr int kMaxMenu = 8;
inline constexpr int kMaxShelf = 4;
inline constexpr int kMaxItems = 1 + kMaxShelf + kMaxMenu;
inline constexpr int kMaxRows = kMaxItems;

struct Box {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  constexpr bool empty() const { return w <= 0 || h <= 0; }
};

// ---- Navigation --------------------------------------------------------------------------------

// Items [rowStart[r], rowStart[r + 1]) form row r.
struct Rows {
  uint8_t start[kMaxRows + 1] = {};
  uint8_t count = 0;
  int total() const { return count == 0 ? 0 : start[count]; }
  int rowOf(const int item) const {
    for (int r = 0; r < count; ++r) {
      if (item < start[r + 1]) return r;
    }
    return count - 1;
  }
};

// `current` is 1 when there is a book being read, `shelf` the shelf covers after it (C only).
inline Rows rowsFor(const Variant v, int current, int shelf, int menu) {
  current = std::clamp(current, 0, 1);
  shelf = v == Variant::C ? std::clamp(shelf, 0, kMaxShelf) : 0;
  menu = std::clamp(menu, 0, kMaxMenu);
  Rows rows;
  int at = 0;
  const auto addRow = [&](const int len) {
    if (len <= 0) return;
    rows.start[rows.count] = static_cast<uint8_t>(at);
    at += len;
    rows.count++;
    rows.start[rows.count] = static_cast<uint8_t>(at);
  };
  addRow(current);
  switch (v) {
    case Variant::A:
      for (int i = 0; i < menu; i += 2) addRow(std::min(2, menu - i));
      break;
    case Variant::B:
      addRow(menu);
      break;
    case Variant::C:
      addRow(shelf);
      for (int i = 0; i < menu; ++i) addRow(1);
      break;
  }
  return rows;
}

enum class Move : uint8_t { Prev, Next, Up, Down };

// The selector after a press. Prev/Next walk the items in order and wrap; Up/Down go to the row
// above or below (wrapping) and keep the column as nearly as the new row allows.
inline int move(const Rows& rows, const int index, const Move m) {
  const int total = rows.total();
  if (total <= 0) return 0;
  const int i = std::clamp(index, 0, total - 1);
  switch (m) {
    case Move::Prev:
      return (i + total - 1) % total;
    case Move::Next:
      return (i + 1) % total;
    case Move::Up:
    case Move::Down: {
      if (rows.count <= 1) return i;
      const int r = rows.rowOf(i);
      const int len = rows.start[r + 1] - rows.start[r];
      const int col = i - rows.start[r];
      const int nr = m == Move::Up ? (r + rows.count - 1) % rows.count : (r + 1) % rows.count;
      const int nlen = rows.start[nr + 1] - rows.start[nr];
      // Proportional: column 1 of two tiles lands on column 1 of the next two, and anything over a
      // single-item row lands on its one item.
      const int ncol = std::min(nlen - 1, col * nlen / std::max(1, len));
      return rows.start[nr] + ncol;
    }
  }
  return i;
}

// ---- Geometry ----------------------------------------------------------------------------------

// Line heights of the three UI faces as the renderer has them (they grow with the UI font size).
struct Fonts {
  int title = 25;  // UI_12
  int body = 20;   // UI_10
  int small = 17;  // SMALL
};

struct Frame {
  // The band the screen may use: under the battery line, above the button hints.
  int top = 0;
  int bottom = 0;
  int width = 0;

  // Small caption above the card (A) / greeting (C).
  Box caption;
  // The current-book block: A's card, B's cover-plus-text, C's row. Empty without a book.
  Box book;
  Box cover;    // the current book's cover slot
  Box text;     // where the title block starts (x, y) and how wide it may run
  Box bar;      // progress bar
  Box status;   // the "41% ~1h 03m left" line (B: the line under the bar)
  Box history;  // "Read 43m over 1 day" (A: inside the card, B: centred)
  Box rule;     // B: the thin rule between book and dock

  Box shelfLabel;  // C
  Box shelf[kMaxShelf];
  int shelfCount = 0;

  Box menu[kMaxMenu];
  int menuCount = 0;
  bool menuHasLeadIcon = true;  // C draws icon + label + chevron in a row
  int iconSize = 32;
};

// The cover height each variant asks the cover pipeline for. The pipeline writes one thumbnail per
// height and book (thumb_<h>.bmp, cropped to 0.6 h wide: CoverThumbLoader::configureSingleHeight),
// so every slot of a variant uses the same height and the slot is exactly the thumbnail.
inline constexpr int coverHeight(const Variant v) { return v == Variant::A ? 196 : v == Variant::B ? 300 : 144; }
inline constexpr int coverWidth(const Variant v) { return coverHeight(v) * 6 / 10; }

namespace detail {
inline void layOutA(Frame& f, const bool hasBook, const int menu, const Fonts& fonts) {
  const int pad = 16;
  const int innerW = f.width - 2 * pad;
  f.caption = Box{pad + 8, f.top + 6, innerW - 16, fonts.small};
  const int cardY = f.caption.y + fonts.small + 8;
  const int coverH = coverHeight(Variant::A);
  const int cardH = hasBook ? coverH + 32 : 96;
  f.book = Box{pad, cardY, innerW, cardH};
  if (hasBook) {
    f.cover = Box{pad + 16, cardY + 16, coverWidth(Variant::A), coverH};
    const int tx = f.cover.x + f.cover.w + 16;
    f.text = Box{tx, cardY + 22, f.book.x + f.book.w - 18 - tx, 0};
    // Bottom-anchored: % line, bar, then one small line, inside the card.
    f.history = Box{tx, cardY + cardH - 18 - fonts.small, f.text.w, fonts.small};
    f.bar = Box{tx, f.history.y - 10 - 10, f.text.w, 10};
    f.status = Box{tx, f.bar.y - 12 - fonts.title, f.text.w, fonts.title};
  } else {
    f.text = Box{pad + 20, cardY + (cardH - fonts.title - fonts.body - 4) / 2, innerW - 40, 0};
  }

  const int tilesTop = cardY + cardH + 22;
  const int gap = 8;
  const int rows = (menu + 1) / 2;
  if (rows <= 0) return;
  const int avail = f.bottom - tilesTop;
  const int tileH = std::clamp((avail - (rows - 1) * gap) / rows, 40, 172);
  const int tileW = (innerW - gap) / 2;
  for (int i = 0; i < menu; ++i) {
    const int row = i / 2;
    const int col = i % 2;
    const bool alone = col == 0 && i == menu - 1;  // an odd last entry spans the row
    f.menu[i] = Box{pad + col * (tileW + gap), tilesTop + row * (tileH + gap), alone ? innerW : tileW, tileH};
  }
  f.menuCount = menu;
  f.iconSize = tileH >= 120 ? 48 : 32;
}

inline void layOutB(Frame& f, const bool hasBook, const int menu, const Fonts& fonts) {
  const int coverH = coverHeight(Variant::B);
  const int coverW = coverWidth(Variant::B);
  const int y0 = f.top + 6;
  if (hasBook) {
    f.book = Box{(f.width - coverW) / 2 - 8, y0 - 8, coverW + 16, coverH + 16};
    f.cover = Box{(f.width - coverW) / 2, y0, coverW, coverH};
    int y = f.cover.y + coverH + 14;
    f.text = Box{40, y, f.width - 80, 0};  // title, centred; author · series below
    y += fonts.title + 4 + fonts.body + 12;
    f.bar = Box{60, y, f.width - 120, 10};
    f.status = Box{60, f.bar.y + 10 + 8, f.width - 120, fonts.small};
    f.history = Box{40, f.status.y + fonts.small + 10, f.width - 80, fonts.small};
    f.rule = Box{40, f.history.y + fonts.small + 16, f.width - 80, 1};
  } else {
    f.cover = Box{(f.width - coverW) / 2, y0, coverW, coverH};
    f.text = Box{40, y0 + coverH + 14, f.width - 80, 0};
    f.rule = Box{40, f.text.y + fonts.title + fonts.body + 30, f.width - 80, 1};
  }
  if (menu <= 0) return;
  const int dockTop = f.rule.y + 24;
  const int pitch = f.width / menu;
  const int itemW = std::min(104, pitch - 8);
  const int itemH = std::min(142, std::max(fonts.body + 50, f.bottom - dockTop - 8));
  for (int i = 0; i < menu; ++i) {
    const int cx = pitch * i + pitch / 2;
    f.menu[i] = Box{cx - itemW / 2, dockTop, itemW, itemH};
  }
  f.menuCount = menu;
  f.iconSize = pitch >= 100 ? 40 : 32;
}

inline void layOutC(Frame& f, const bool hasBook, const int shelf, const int menu, const Fonts& fonts) {
  const int pad = 24;
  f.caption = Box{pad, f.top + 4, f.width - 2 * pad, fonts.title};
  int y = f.caption.y + fonts.title + 18;
  const int coverH = coverHeight(Variant::C);
  const int coverW = coverWidth(Variant::C);
  if (hasBook) {
    f.book = Box{pad - 8, y - 8, f.width - 2 * pad + 16, coverH + 16};
    f.cover = Box{pad, y, coverW, coverH};
    const int tx = pad + coverW + 16;
    f.text = Box{tx, y + 4, f.width - pad - tx, 0};
    f.status = Box{tx, y + coverH - 6 - fonts.small, f.text.w, fonts.small};
    f.bar = Box{tx, f.status.y - 8 - 8, f.text.w, 8};
    y += coverH + 22;
  } else {
    f.text = Box{pad, y, f.width - 2 * pad, 0};
    y += fonts.title + fonts.body + 24;
  }
  if (shelf > 0) {
    f.shelfLabel = Box{pad, y, f.width - 2 * pad, fonts.small};
    y += fonts.small + 10;
    const int pitch = (f.width - 2 * pad - coverW) / std::max(1, kMaxShelf - 1);
    for (int i = 0; i < shelf && i < kMaxShelf; ++i) f.shelf[i] = Box{pad + i * pitch, y, coverW, coverH};
    f.shelfCount = std::min(shelf, kMaxShelf);
    y += coverH + 20;
  }
  if (menu <= 0) return;
  const int pitch = std::clamp((f.bottom - y) / menu, 36, 72);
  const int rowH = pitch - (pitch >= 56 ? 10 : 4);
  for (int i = 0; i < menu; ++i) f.menu[i] = Box{16, y + i * pitch, f.width - 32, rowH};
  f.menuCount = menu;
  f.iconSize = 32;
}
}  // namespace detail

inline Frame frameFor(const Variant v, const int width, const int top, const int bottom, const bool hasBook,
                      const int shelf, int menu, const Fonts& fonts) {
  Frame f;
  f.width = width;
  f.top = top;
  f.bottom = bottom;
  menu = std::clamp(menu, 0, kMaxMenu);
  switch (v) {
    case Variant::A:
      detail::layOutA(f, hasBook, menu, fonts);
      break;
    case Variant::B:
      detail::layOutB(f, hasBook, menu, fonts);
      break;
    case Variant::C:
      detail::layOutC(f, hasBook, std::clamp(shelf, 0, kMaxShelf), menu, fonts);
      break;
  }
  return f;
}

// Width of the filled part of a progress bar of `width` px at `percent` (clamped), at least the
// bar's height once anything is read so the rounded fill never collapses to a sliver.
inline int barFill(const int width, const int height, const int percent) {
  if (percent <= 0 || width <= 0) return 0;
  const int w = width * std::min(percent, 100) / 100;
  return std::clamp(w, std::min(height, width), width);
}

}  // namespace OrbitHomeLayout
