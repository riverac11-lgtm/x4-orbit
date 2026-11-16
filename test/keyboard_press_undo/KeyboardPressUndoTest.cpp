#include <gtest/gtest.h>

#include <string>

#include "KeyboardPressUndo.h"

namespace {

// What KeyboardEntryActivity does around a press: arm, then insert at the cursor and (for a
// letter) release Shift. Mirrors insertUtf8()'s rules, including the length cap.
struct Field {
  std::string text;
  size_t cursor = 0;
  size_t typedEnd = std::string::npos;
  bool shifted = false;
  size_t maxLength = 0;

  void press(KeyboardPressUndo& undo, const char* out) {
    undo.arm(text, cursor, typedEnd, shifted);
    const std::string s(out);
    if (maxLength == 0 || text.length() + s.length() <= maxLength) {
      text.insert(cursor, s);
      cursor += s.length();
      typedEnd = cursor;
    }
    shifted = false;
  }
  bool revert(KeyboardPressUndo& undo) { return undo.revert(text, cursor, typedEnd, shifted); }
};

TEST(KeyboardPressUndo, RevertsATypedLetterAtTheEnd) {
  Field f{"ab", 2};
  KeyboardPressUndo undo;
  f.press(undo, "c");
  ASSERT_EQ(f.text, "abc");
  ASSERT_TRUE(f.revert(undo));
  EXPECT_EQ(f.text, "ab");
  EXPECT_EQ(f.cursor, 2u);
  EXPECT_EQ(f.typedEnd, std::string::npos);
}

TEST(KeyboardPressUndo, RevertsInTheMiddleAndRestoresShift) {
  Field f{"hllo", 1};
  f.shifted = true;
  f.typedEnd = 4;
  KeyboardPressUndo undo;
  f.press(undo, "E");
  ASSERT_EQ(f.text, "hEllo");
  ASSERT_FALSE(f.shifted);
  ASSERT_TRUE(f.revert(undo));
  EXPECT_EQ(f.text, "hllo");
  EXPECT_EQ(f.cursor, 1u);
  EXPECT_EQ(f.typedEnd, 4u);
  EXPECT_TRUE(f.shifted);
}

TEST(KeyboardPressUndo, RevertsAMultiByteCharacterWhole) {
  Field f{"x", 1};
  KeyboardPressUndo undo;
  f.press(undo, "\xD0\xB6");  // Cyrillic zhe, two bytes
  ASSERT_TRUE(f.revert(undo));
  EXPECT_EQ(f.text, "x");
  EXPECT_EQ(f.cursor, 1u);
}

TEST(KeyboardPressUndo, RevertsAMultiCharacterSnippet) {
  Field f{"", 0};
  KeyboardPressUndo undo;
  f.press(undo, "https://");
  ASSERT_TRUE(f.revert(undo));
  EXPECT_EQ(f.text, "");
  EXPECT_EQ(f.cursor, 0u);
}

TEST(KeyboardPressUndo, AFullFieldInsertsNothingButShiftStillComesBack) {
  Field f{"abc", 3};
  f.maxLength = 3;
  f.shifted = true;
  KeyboardPressUndo undo;
  f.press(undo, "D");
  ASSERT_EQ(f.text, "abc");
  ASSERT_TRUE(f.revert(undo));
  EXPECT_EQ(f.text, "abc");
  EXPECT_EQ(f.cursor, 3u);
  EXPECT_TRUE(f.shifted);
}

TEST(KeyboardPressUndo, OnlyOncePerArm) {
  Field f{"", 0};
  KeyboardPressUndo undo;
  f.press(undo, "a");
  ASSERT_TRUE(f.revert(undo));
  EXPECT_FALSE(undo.armed);
  EXPECT_FALSE(f.revert(undo));
  EXPECT_EQ(f.text, "");
}

TEST(KeyboardPressUndo, DisarmedChangesNothing) {
  Field f{"ab", 2};
  KeyboardPressUndo undo;
  f.press(undo, "c");
  undo.disarm();
  EXPECT_FALSE(f.revert(undo));
  EXPECT_EQ(f.text, "abc");
  EXPECT_EQ(f.cursor, 3u);
}

TEST(KeyboardPressUndo, RefusesWhenTheTextShrankSince) {
  // Something else (a clear) edited the field after the press: leave it alone.
  Field f{"abc", 3};
  KeyboardPressUndo undo;
  f.press(undo, "d");
  f.text.clear();
  f.cursor = 0;
  EXPECT_FALSE(f.revert(undo));
  EXPECT_EQ(f.text, "");
  EXPECT_EQ(f.cursor, 0u);
}

}  // namespace
