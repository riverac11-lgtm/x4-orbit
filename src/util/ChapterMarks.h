#pragma once

#include <cstdint>

// Where the chapters start along a book, for the ticks on the reader's book progress bar.
//
// Positions are book fractions in 1/65535ths, added in reading order. At most kMax are kept: a book
// with more chapters is thinned as it streams in (every other kept mark dropped, and from then on
// only every second, fourth, ... start taken), so a 300-chapter book still gets ticks spread over
// its whole length from one pass and 130 bytes.
struct ChapterMarks {
  static constexpr uint8_t kMax = 64;
  static constexpr uint16_t kFull = 65535;

  uint8_t count = 0;
  uint16_t at[kMax] = {};

  void clear() {
    count = 0;
    stride_ = 1;
    seen_ = 0;
  }
  // A chapter start. Ignores the very start and end of the book, and a start not after the last
  // one kept (several TOC entries in one file share its start).
  void add(uint16_t position);

  // The x offsets (0..width-1) of the ticks on a bar `width` px wide, at least `minGap` px apart;
  // returns how many were written to `out` (room for kMax).
  int tickOffsets(int width, int minGap, int16_t* out) const;

 private:
  uint16_t stride_ = 1;
  uint16_t seen_ = 0;
};
