#pragma once

// The on-screen keyboard types a character when Confirm goes DOWN, not when it comes back up:
// every refresh costs about half a second on e-ink, so starting it a press-length earlier is
// the one part of a keystroke's latency the firmware controls. Holding Confirm still means
// "the other character" (upper case, an accent), so when a press turns into a hold this puts
// the field back the way the press found it before the alternate goes in.
//
// Pure (std::string only) so the host tests cover it; the activity owns the text and calls in.

#include <cstddef>
#include <string>

struct KeyboardPressUndo {
  size_t pos = 0;  // where the press inserted (the cursor before it)
  size_t lengthBefore = 0;
  size_t typedEnd = std::string::npos;
  bool shifted = false;  // a typed letter releases Shift; the hold needs the layer it was pressed on
  bool armed = false;

  // Call right BEFORE the press inserts, with the field as it stands.
  void arm(const std::string& text, const size_t cursorPos, const size_t typedEndBefore, const bool shiftedBefore) {
    pos = cursorPos;
    lengthBefore = text.length();
    typedEnd = typedEndBefore;
    shifted = shiftedBefore;
    armed = true;
  }

  void disarm() { armed = false; }

  // Takes back what the press inserted (nothing, when the field was full) and restores the cursor,
  // the password reveal and Shift. False, changing nothing, when not armed or when the text no
  // longer looks like the press left it (something else edited it since).
  bool revert(std::string& text, size_t& cursorPos, size_t& typedEndOut, bool& shiftedOut) {
    if (!armed) return false;
    armed = false;
    if (text.length() < lengthBefore || pos > lengthBefore) return false;
    const size_t inserted = text.length() - lengthBefore;
    if (inserted > 0) text.erase(pos, inserted);
    cursorPos = pos;
    typedEndOut = typedEnd;
    shiftedOut = shifted;
    return true;
  }
};
