#include <gtest/gtest.h>

#include "components/themes/orbit/OrbitChrome.h"

using OrbitChrome::parseHintLabel;

TEST(OrbitChrome, BackButtonLosesItsMark) {
  const auto l = parseHintLabel("\xC2\xAB Back", true);
  EXPECT_EQ(l.text, "Back");
  EXPECT_EQ(l.chevron, 0);
}

TEST(OrbitChrome, PagingMarksBecomeChevronsOnTheirSide) {
  auto l = parseHintLabel("\xC2\xAB Page", false);
  EXPECT_EQ(l.text, "Page");
  EXPECT_EQ(l.chevron, -1);
  EXPECT_FALSE(l.chevronAfter);

  l = parseHintLabel("Page \xC2\xBB", false);
  EXPECT_EQ(l.text, "Page");
  EXPECT_EQ(l.chevron, 1);
  EXPECT_TRUE(l.chevronAfter);
}

TEST(OrbitChrome, ABareMarkStaysAsAChevronSoTheButtonKeepsAHint) {
  const auto l = parseHintLabel("\xC2\xBB", false);
  EXPECT_TRUE(l.text.empty());
  EXPECT_EQ(l.chevron, 1);
  EXPECT_FALSE(l.empty());
  // Even on the Back key a bare mark is all the hint there is.
  EXPECT_FALSE(parseHintLabel("\xC2\xAB", true).empty());
}

TEST(OrbitChrome, EmptyAndPlainLabels) {
  EXPECT_TRUE(parseHintLabel("", false).empty());
  EXPECT_TRUE(parseHintLabel(nullptr, false).empty());
  EXPECT_TRUE(parseHintLabel("   ", false).empty());
  const auto l = parseHintLabel("Open", false);
  EXPECT_EQ(l.text, "Open");
  EXPECT_EQ(l.chevron, 0);
}

TEST(OrbitChrome, OptionsAfterAPageMarkKeepsBoth) {
  const auto l = parseHintLabel("\xC2\xBB / Options", false);
  EXPECT_EQ(l.text, "/ Options");
  EXPECT_EQ(l.chevron, 1);
}
