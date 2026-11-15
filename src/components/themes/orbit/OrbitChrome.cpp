#include "OrbitChrome.h"

#include <GfxRenderer.h>

#include <cstring>

#include "OrbitHomeDraw.h"

namespace OrbitChrome {

namespace {
constexpr const char* kLeftMark = "\xC2\xAB";   // «
constexpr const char* kRightMark = "\xC2\xBB";  // »
constexpr int kRadius = 6;

int8_t markAt(const std::string& s, const size_t pos) {
  if (s.compare(pos, 2, kLeftMark) == 0) return -1;
  if (s.compare(pos, 2, kRightMark) == 0) return 1;
  return 0;
}

void trim(std::string& s) {
  const size_t first = s.find_first_not_of(' ');
  if (first == std::string::npos) {
    s.clear();
    return;
  }
  s.erase(0, first);
  s.erase(s.find_last_not_of(' ') + 1);
}
}  // namespace

HintLabel parseHintLabel(const char* label, const bool backButton) {
  HintLabel out;
  if (label == nullptr || label[0] == '\0') return out;
  out.text = label;
  trim(out.text);
  if (out.text.size() >= 2) {
    if (const int8_t lead = markAt(out.text, 0)) {
      out.chevron = lead;
      out.text.erase(0, 2);
    } else if (const int8_t tail = markAt(out.text, out.text.size() - 2)) {
      out.chevron = tail;
      out.chevronAfter = true;
      out.text.erase(out.text.size() - 2);
    }
    trim(out.text);
  }
  if (backButton && !out.text.empty()) out.chevron = 0;
  return out;
}

int contentWidth(const GfxRenderer& renderer, const HintLabel& label, const int fontId) {
  int w = label.text.empty() ? 0 : renderer.getTextWidth(fontId, label.text.c_str());
  if (label.chevron != 0) w += kChevronWidth + (label.text.empty() ? 0 : kChevronGap);
  return w;
}

void drawHintBox(const GfxRenderer& renderer, const int x, const int y, const int w, const int h,
                 const HintLabel& label, const int fontId, const bool inverted) {
  renderer.fillRoundedRect(x, y, w, h, kRadius, Color::White);
  renderer.drawRoundedRect(x, y, w, h, 1, kRadius, !inverted, !inverted, inverted, inverted, true);

  const int lineH = renderer.getLineHeight(fontId);
  // The text sits where Lyra's does, 7 px under the box top (in either orientation: an inverted
  // strip is drawn in the inverted frame, where the box is again below its text).
  constexpr int textYOffset = 7;
  const int textY = y + textYOffset;
  const int content = contentWidth(renderer, label, fontId);
  int cx = x + (w - content) / 2;
  const int chevronY = textY + lineH / 2 + 1;
  const auto chevron = [&]() {
    OrbitHomeDraw::drawChevron(renderer, cx + kChevronWidth / 2, chevronY, 10, label.chevron > 0);
    cx += kChevronWidth + kChevronGap;
  };
  if (label.chevron != 0 && !label.chevronAfter) chevron();
  if (!label.text.empty()) {
    renderer.drawText(fontId, cx, textY, label.text.c_str());
    cx += renderer.getTextWidth(fontId, label.text.c_str()) + kChevronGap;
  }
  if (label.chevron != 0 && label.chevronAfter) chevron();
}

}  // namespace OrbitChrome
