#include "ChapterMarks.h"

namespace {
// The first and last half percent of a book carry no tick: one there would sit on the bar's end.
constexpr uint16_t kEdge = ChapterMarks::kFull / 200;
}  // namespace

void ChapterMarks::add(const uint16_t position) {
  if (position <= kEdge || position >= kFull - kEdge) return;
  if (count > 0 && position <= at[count - 1]) return;
  if (seen_++ % stride_ != 0) return;
  if (count == kMax) {
    // Full: keep every other mark and take only every other start from here on.
    for (uint8_t i = 0; i < kMax / 2; ++i) at[i] = at[i * 2 + 1];
    count = kMax / 2;
    stride_ = static_cast<uint16_t>(stride_ * 2);
    seen_ = 1;
  }
  at[count++] = position;
}

int ChapterMarks::tickOffsets(const int width, const int minGap, int16_t* out) const {
  if (width <= 0) return 0;
  int n = 0;
  int last = -minGap;
  for (uint8_t i = 0; i < count; ++i) {
    const int x = static_cast<int>(static_cast<uint32_t>(at[i]) * static_cast<uint32_t>(width - 1) / kFull);
    if (x - last < minGap) continue;
    out[n++] = static_cast<int16_t>(x);
    last = x;
  }
  return n;
}
