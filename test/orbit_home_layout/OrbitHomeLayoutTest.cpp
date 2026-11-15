#include <gtest/gtest.h>

#include "components/themes/orbit/OrbitHomeLayout.h"

using namespace OrbitHomeLayout;

namespace {
const Fonts kFonts{};
constexpr int kTop = 30;
constexpr int kBottom = 752;

bool inside(const Box& b, const int top, const int bottom, const int width) {
  return b.x >= 0 && b.y >= top && b.x + b.w <= width && b.y + b.h <= bottom;
}
bool overlap(const Box& a, const Box& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
}  // namespace

// ---- Navigation ----------------------------------------------------------------------------------

TEST(OrbitHomeNav, CardAndTwoByTwoTiles) {
  const Rows rows = rowsFor(Variant::A, 1, 0, 4);
  ASSERT_EQ(rows.count, 3);
  EXPECT_EQ(rows.total(), 5);
  // Down from the card lands on the first tile, then straight down the column.
  EXPECT_EQ(move(rows, 0, Move::Down), 1);
  EXPECT_EQ(move(rows, 2, Move::Down), 4);
  EXPECT_EQ(move(rows, 1, Move::Down), 3);
  // Up from either top tile goes back to the card; up from the card wraps to the bottom row.
  EXPECT_EQ(move(rows, 2, Move::Up), 0);
  EXPECT_EQ(move(rows, 1, Move::Up), 0);
  EXPECT_EQ(move(rows, 0, Move::Up), 3);
  // Left/Right walk everything in order, wrapping.
  EXPECT_EQ(move(rows, 4, Move::Next), 0);
  EXPECT_EQ(move(rows, 0, Move::Prev), 4);
  EXPECT_EQ(move(rows, 1, Move::Next), 2);
}

TEST(OrbitHomeNav, OddTileCountEndsInAOneTileRow) {
  const Rows rows = rowsFor(Variant::A, 1, 0, 5);
  ASSERT_EQ(rows.count, 4);
  EXPECT_EQ(move(rows, 4, Move::Down), 5);  // right column into the lone last tile
  EXPECT_EQ(move(rows, 5, Move::Down), 0);  // and on round to the card
}

TEST(OrbitHomeNav, NoBookLeavesOnlyTheMenu) {
  const Rows rows = rowsFor(Variant::A, 0, 0, 4);
  EXPECT_EQ(rows.total(), 4);
  EXPECT_EQ(move(rows, 0, Move::Down), 2);
}

TEST(OrbitHomeNav, CoverAndDockToggleWithUpDown) {
  const Rows rows = rowsFor(Variant::B, 1, 3, 4);  // a shelf means nothing to B
  ASSERT_EQ(rows.count, 2);
  EXPECT_EQ(rows.total(), 5);
  EXPECT_EQ(move(rows, 0, Move::Down), 1);
  EXPECT_EQ(move(rows, 3, Move::Up), 0);
  EXPECT_EQ(move(rows, 0, Move::Next), 1);
  EXPECT_EQ(move(rows, 4, Move::Next), 0);
}

TEST(OrbitHomeNav, ShelfIsOneRowBetweenBookAndList) {
  const Rows rows = rowsFor(Variant::C, 1, 4, 4);
  ASSERT_EQ(rows.count, 6);
  EXPECT_EQ(rows.total(), 9);
  EXPECT_EQ(move(rows, 0, Move::Down), 1);  // book -> first shelf cover
  EXPECT_EQ(move(rows, 3, Move::Down), 5);  // any shelf cover -> first menu row
  EXPECT_EQ(move(rows, 5, Move::Up), 1);    // first menu row -> shelf
  EXPECT_EQ(move(rows, 2, Move::Next), 3);  // across the shelf
  EXPECT_EQ(move(rows, 8, Move::Down), 0);  // wraps
}

TEST(OrbitHomeNav, EmptyIsHarmless) {
  const Rows rows = rowsFor(Variant::C, 0, 0, 0);
  EXPECT_EQ(rows.total(), 0);
  EXPECT_EQ(move(rows, 3, Move::Down), 0);
}

// ---- Geometry ------------------------------------------------------------------------------------

TEST(OrbitHomeFrame, EveryVariantFitsTheBandWithoutOverlaps) {
  for (const Variant v : {Variant::A, Variant::B, Variant::C}) {
    for (int menu = 1; menu <= kMaxMenu; ++menu) {
      for (const bool book : {true, false}) {
        const Frame f = frameFor(v, 480, kTop, kBottom, book, 4, menu, kFonts);
        SCOPED_TRACE(testing::Message() << "variant " << int(v) << " menu " << menu << " book " << book);
        ASSERT_EQ(f.menuCount, menu);
        for (int i = 0; i < menu; ++i) {
          EXPECT_TRUE(inside(f.menu[i], kTop, kBottom, 480));
          for (int j = i + 1; j < menu; ++j) EXPECT_FALSE(overlap(f.menu[i], f.menu[j]));
          if (book) EXPECT_FALSE(overlap(f.menu[i], f.book));
          for (int s = 0; s < f.shelfCount; ++s) EXPECT_FALSE(overlap(f.menu[i], f.shelf[s]));
        }
        if (book) EXPECT_TRUE(inside(f.cover, kTop, kBottom, 480));
      }
    }
  }
}

TEST(OrbitHomeFrame, MockupProportionsAtFourEntries) {
  const Frame a = frameFor(Variant::A, 480, kTop, kBottom, true, 0, 4, kFonts);
  EXPECT_EQ(a.menu[0].w, a.menu[1].w);
  EXPECT_EQ(a.menu[0].y, a.menu[1].y);
  EXPECT_GE(a.menu[0].h, 150);  // roomy tiles, as drawn
  EXPECT_EQ(a.cover.h, coverHeight(Variant::A));

  const Frame b = frameFor(Variant::B, 480, kTop, kBottom, true, 0, 4, kFonts);
  EXPECT_EQ(b.menu[0].x + b.menu[0].w / 2, 60);  // dock centres at 60, 180, 300, 420
  EXPECT_EQ(b.menu[3].x + b.menu[3].w / 2, 420);
  EXPECT_EQ(b.cover.x * 2 + b.cover.w, 480);  // the cover is centred

  const Frame c = frameFor(Variant::C, 480, kTop, kBottom, true, 4, 4, kFonts);
  ASSERT_EQ(c.shelfCount, 4);
  EXPECT_EQ(c.shelf[0].x, 24);
  EXPECT_NEAR(c.shelf[3].x + c.shelf[3].w, 480 - 24, 2);
  EXPECT_GE(c.menu[1].y - c.menu[0].y, 60);
}

TEST(OrbitHomeFrame, OddLastTileSpansTheRow) {
  const Frame a = frameFor(Variant::A, 480, kTop, kBottom, true, 0, 3, kFonts);
  EXPECT_EQ(a.menu[2].x, a.menu[0].x);
  EXPECT_EQ(a.menu[2].w, a.menu[1].x + a.menu[1].w - a.menu[0].x);
}

TEST(OrbitHomeFrame, BarFill) {
  EXPECT_EQ(barFill(300, 10, -1), 0);
  EXPECT_EQ(barFill(300, 10, 0), 0);
  EXPECT_EQ(barFill(300, 10, 1), 10);  // never thinner than the bar is tall
  EXPECT_EQ(barFill(300, 10, 41), 123);
  EXPECT_EQ(barFill(300, 10, 150), 300);
}
