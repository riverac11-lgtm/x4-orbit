#include <gtest/gtest.h>

#include <string>

#include "ChargeWarmState.h"

using namespace ChargeWarm;

namespace {
Frame x4Portrait() {
  Frame f;
  f.panelWidth = 800;
  f.panelHeight = 480;
  f.panelWidthBytes = 100;
  f.orientation = 0;
  f.grayscale = 1;
  f.left = 13;
  f.top = 27;
  f.viewportHeight = 740;
  return f;
}
}  // namespace

TEST(ChargeWarmState, HashFromSectionPath) {
  uint32_t hash = 0;
  EXPECT_TRUE(hashFromSectionPath("/.crosspoint/epub_123/s0/12_0a1b2c3d.bin", hash));
  EXPECT_EQ(hash, 0x0a1b2c3du);
  EXPECT_TRUE(hashFromSectionPath("0_FFFFFFFF.bin", hash));
  EXPECT_EQ(hash, 0xffffffffu);
  EXPECT_FALSE(hashFromSectionPath("/x/html_12.bin", hash));
  EXPECT_FALSE(hashFromSectionPath("/x/12_0a1b2c3.bin", hash));   // 7 hex digits
  EXPECT_FALSE(hashFromSectionPath("/x/12_0a1b2c3g.bin", hash));  // not hex
  EXPECT_FALSE(hashFromSectionPath("/x/_0a1b2c3d.bin", hash));    // no spine
  EXPECT_FALSE(hashFromSectionPath("/x/1a_0a1b2c3d.bin", hash));  // spine not a number
  EXPECT_FALSE(hashFromSectionPath("/x/12_0a1b2c3d.tmp", hash));
  EXPECT_FALSE(hashFromSectionPath("", hash));
}

TEST(ChargeWarmState, WindowIsTheChapterAndTwoAfter) {
  const State s = fromReader("/Books/a.epub", x4Portrait(), 7, 4, 9, 20);
  EXPECT_EQ(s.spine, 4);
  EXPECT_EQ(s.page, 9);
  EXPECT_EQ(s.endSpine, 4 + 1 + kSpinesAhead);
  EXPECT_TRUE(pending(s));
}

TEST(ChargeWarmState, WindowStopsAtTheLastSpine) {
  const State s = fromReader("/Books/a.epub", x4Portrait(), 7, 18, 0, 20);
  EXPECT_EQ(s.endSpine, 20);
  State last = fromReader("/Books/a.epub", x4Portrait(), 7, 19, 3, 20);
  EXPECT_EQ(last.endSpine, 20);
  EXPECT_TRUE(pending(last));
  nextSpine(last);
  EXPECT_FALSE(pending(last));
  EXPECT_EQ(last.page, 0);
  nextSpine(last);  // never past the end
  EXPECT_EQ(last.spine, 20);
}

TEST(ChargeWarmState, NothingPendingForAFinishedOrBadPosition) {
  EXPECT_FALSE(pending(fromReader("/Books/a.epub", x4Portrait(), 7, 20, 0, 20)));  // past the last spine
  EXPECT_FALSE(pending(fromReader("/Books/a.epub", x4Portrait(), 7, -1, 0, 20)));
  EXPECT_FALSE(pending(fromReader("/Books/a.epub", x4Portrait(), 7, 2, -1, 20)));
  EXPECT_FALSE(pending(fromReader("", x4Portrait(), 7, 2, 0, 20)));
}

TEST(ChargeWarmState, CompletedBooksAreSkipped) {
  EXPECT_TRUE(inCompleted("/COMPLETED/a.epub"));
  EXPECT_TRUE(inCompleted("/COMPLETED"));
  EXPECT_FALSE(inCompleted("/COMPLETEDX/a.epub"));
  EXPECT_FALSE(inCompleted("/Books/COMPLETED/a.epub"));
  EXPECT_FALSE(inCompleted("/completed/a.epub"));  // the folder the reader moves to is upper case
  EXPECT_FALSE(pending(fromReader("/COMPLETED/a.epub", x4Portrait(), 7, 2, 0, 20)));
}

TEST(ChargeWarmState, FrameSanity) {
  EXPECT_TRUE(frameUsable(x4Portrait()));
  Frame x3 = x4Portrait();
  x3.panelWidth = 792;
  x3.panelHeight = 528;
  x3.panelWidthBytes = 99;
  EXPECT_TRUE(frameUsable(x3));
  Frame f = x4Portrait();
  f.panelWidth = 0;  // a renderer never begun
  EXPECT_FALSE(frameUsable(f));
  f = x4Portrait();
  f.panelWidthBytes = 50;  // too narrow for the width
  EXPECT_FALSE(frameUsable(f));
  f = x4Portrait();
  f.orientation = 4;
  EXPECT_FALSE(frameUsable(f));
  f = x4Portrait();
  f.left = -1;
  EXPECT_FALSE(frameUsable(f));
  f = x4Portrait();
  f.grayscale = 2;
  EXPECT_FALSE(frameUsable(f));
  EXPECT_FALSE(pending(fromReader("/Books/a.epub", Frame{}, 7, 2, 0, 20)));
}

TEST(ChargeWarmState, RoundTrip) {
  State s = fromReader("/Books/Some author/A long title.epub", x4Portrait(), 0xdeadbeef, 5, 12, 40);
  nextSpine(s);
  s.page = 3;
  std::string data;
  encode(s, data);
  State back;
  ASSERT_TRUE(decode(data, back));
  EXPECT_EQ(back.bookPath, s.bookPath);
  EXPECT_EQ(back.sectionHash, 0xdeadbeefu);
  EXPECT_EQ(back.spine, 6);
  EXPECT_EQ(back.page, 3);
  EXPECT_EQ(back.endSpine, s.endSpine);
  EXPECT_EQ(back.frame.left, 13);
  EXPECT_EQ(back.frame.top, 27);
  EXPECT_EQ(back.frame.viewportHeight, 740);
  EXPECT_EQ(back.frame.panelWidthBytes, 100);
  EXPECT_EQ(back.frame.grayscale, 1);
}

TEST(ChargeWarmState, RejectsDamagedFiles) {
  State s = fromReader("/Books/a.epub", x4Portrait(), 1, 2, 0, 20);
  std::string data;
  encode(s, data);
  State out;
  EXPECT_FALSE(decode("", out));
  EXPECT_FALSE(decode(data.substr(0, data.size() - 1), out));  // path cut short
  EXPECT_FALSE(decode(data + "x", out));                       // trailing bytes
  std::string bad = data;
  bad[0] ^= 0x55;  // magic
  EXPECT_FALSE(decode(bad, out));
  EXPECT_TRUE(out.bookPath.empty());
  EXPECT_FALSE(pending(out));
}
