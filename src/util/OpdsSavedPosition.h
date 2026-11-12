#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Where the OPDS browser was when it was last left, so the next visit to the same server opens
// there: the feed on screen, the feeds Back leads through (oldest first, the catalog root "" at the
// bottom) and the selected row. Pure logic, no SD card and no Arduino (host tests: test/opds_browse).
//
// Stored as text, one value per line:  OPDSPOS1 / row / feed / history[0] / history[1] / ...
// A file that is cut short, too long or of another version decodes to nothing, and the browser
// opens at the root as it always did.
struct OpdsSavedPosition {
  std::string feed;
  std::vector<std::string> history;
  uint16_t row = 0;
};

namespace OpdsPosition {
// Back steps kept. A browse deeper than this (paging through "next" links adds one each) keeps
// the root and the most recent ones, so Back still ends at the catalog root.
constexpr size_t kMaxDepth = 16;
constexpr size_t kMaxUrlLength = 1024;
// What the browser will read back: kMaxDepth + 1 URLs and the two short lines.
constexpr size_t kMaxFileBytes = (kMaxDepth + 1) * (kMaxUrlLength + 1) + 32;

// False when there is nothing to remember (the root feed with no way back) or a URL cannot be
// stored (too long, or holding a line break); `out` is then empty and the caller removes the file.
bool encode(const std::string& feed, const std::vector<std::string>& history, int row, std::string& out);
bool decode(const std::string& in, OpdsSavedPosition& out);
}  // namespace OpdsPosition
