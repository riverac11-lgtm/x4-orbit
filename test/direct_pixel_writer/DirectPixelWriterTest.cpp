#include <Epub/converters/DirectPixelWriter.h>
#include <gtest/gtest.h>

#include <algorithm>

namespace {

bool allWhite(const GfxRenderer& r) {
  return std::all_of(std::begin(r.frame), std::end(r.frame), [](uint8_t b) { return b == 0xFF; });
}

void writeBlackRow(DirectPixelWriter& pw, int y) {
  pw.beginRow(y);
  for (int x = 0; x < GfxRenderer::kWidth; x++) pw.writePixel(x, 0);  // level 0 = black
}

}  // namespace

TEST(DirectPixelWriterTest, WritesReachTheFrameBuffer) {
  GfxRenderer renderer;
  DirectPixelWriter pw;
  pw.init(renderer);
  writeBlackRow(pw, 3);
  EXPECT_FALSE(allWhite(renderer));
  EXPECT_EQ(renderer.frame[3 * renderer.getDisplayWidthBytes()], 0x00);
}

// Look-ahead decodes run the same decode loops as a visible render but must
// leave the page on screen untouched: suppress() drops every write.
TEST(DirectPixelWriterTest, SuppressedWriterLeavesFrameBufferUntouched) {
  GfxRenderer renderer;
  DirectPixelWriter pw;
  pw.init(renderer);
  pw.suppress();
  for (int y = 0; y < GfxRenderer::kHeight; y++) writeBlackRow(pw, y);
  EXPECT_TRUE(allWhite(renderer));
}

TEST(DirectPixelWriterTest, SuppressedWriterReportsNoBandColumns) {
  GfxRenderer renderer;
  DirectPixelWriter pw;
  pw.init(renderer);
  pw.suppress();
  pw.beginRow(2);
  int colStart = -1;
  int colEnd = -1;
  pw.bandColRange(0, GfxRenderer::kWidth, colStart, colEnd);
  EXPECT_EQ(colStart, colEnd);
}
