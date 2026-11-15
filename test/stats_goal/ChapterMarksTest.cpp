#include <gtest/gtest.h>

#include "util/ChapterMarks.h"

TEST(ChapterMarks, KeepsStartsInOrderAndSkipsTheEnds) {
  ChapterMarks m;
  m.add(0);
  m.add(100);  // inside the first half percent
  m.add(10000);
  m.add(10000);  // two TOC entries in one file
  m.add(9000);   // out of order
  m.add(30000);
  m.add(65500);  // the very end
  ASSERT_EQ(m.count, 2);
  EXPECT_EQ(m.at[0], 10000);
  EXPECT_EQ(m.at[1], 30000);
}

TEST(ChapterMarks, ThinsALongTocOverTheWholeBook) {
  ChapterMarks m;
  constexpr int kChapters = 300;
  for (int i = 1; i < kChapters; ++i) m.add(static_cast<uint16_t>(i * 65535 / kChapters));
  EXPECT_LE(m.count, ChapterMarks::kMax);
  EXPECT_GE(m.count, ChapterMarks::kMax / 2);
  for (int i = 1; i < m.count; ++i) EXPECT_GT(m.at[i], m.at[i - 1]);
  // Spread to both ends, not just the first chapters.
  EXPECT_LT(m.at[0], 65535 / 20);
  EXPECT_GT(m.at[m.count - 1], 65535 - 65535 / 20);
}

TEST(ChapterMarks, TicksKeepTheirDistance) {
  ChapterMarks m;
  for (int i = 1; i < 60; ++i) m.add(static_cast<uint16_t>(i * 1000));
  int16_t out[ChapterMarks::kMax];
  const int n = m.tickOffsets(100, 4, out);
  ASSERT_GT(n, 0);
  for (int i = 1; i < n; ++i) EXPECT_GE(out[i] - out[i - 1], 4);
  for (int i = 0; i < n; ++i) EXPECT_LT(out[i], 100);
  const int wide = m.tickOffsets(480, 4, out);
  EXPECT_EQ(wide, m.count);
  EXPECT_EQ(m.tickOffsets(0, 4, out), 0);

  m.clear();
  EXPECT_EQ(m.count, 0);
  m.add(30000);
  EXPECT_EQ(m.count, 1);
}
