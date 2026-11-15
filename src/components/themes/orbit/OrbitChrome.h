#pragma once

#include <cstdint>
#include <string>

class GfxRenderer;

// The Orbit look of the button-hint boxes, kept apart from the theme so it is testable and the host
// preview draws the same thing.
namespace OrbitChrome {

// A hint label with Witch(hunt)'s "«" / "»" text marks taken out. Where a mark carried meaning (the
// front Left/Right of a list page backwards and forwards: "« Page", "»", "Page »") it becomes a
// drawn chevron on the same side; on the Back button ("« Back", "« Home") the position of the key
// already says it, so it is dropped.
struct HintLabel {
  std::string text;
  int8_t chevron = 0;         // -1 points left, +1 points right, 0 none
  bool chevronAfter = false;  // drawn after the text ("Page ›") rather than before ("‹ Page")
  bool empty() const { return text.empty() && chevron == 0; }
};

HintLabel parseHintLabel(const char* label, bool backButton);

inline constexpr int kChevronWidth = 7;
inline constexpr int kChevronGap = 5;

// Width the label's content needs in `fontId` (text plus chevron), padding excluded.
int contentWidth(const GfxRenderer& renderer, const HintLabel& label, int fontId);

// One bottom hint box at (x, y), `w` x `h`, open on the edge that meets the panel border (the bottom
// one, or the top one when `inverted`), with its content centred.
void drawHintBox(const GfxRenderer& renderer, int x, int y, int w, int h, const HintLabel& label, int fontId,
                 bool inverted);

}  // namespace OrbitChrome
