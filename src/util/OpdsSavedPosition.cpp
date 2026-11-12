#include "OpdsSavedPosition.h"

#include <cstdio>
#include <utility>

namespace OpdsPosition {
namespace {
constexpr char kMagic[] = "OPDSPOS1";

bool storable(const std::string& url) {
  return url.size() <= kMaxUrlLength && url.find_first_of("\r\n") == std::string::npos;
}

// The line starting at `pos`, without its '\n'; false when the text ends before one.
bool nextLine(const std::string& in, size_t& pos, std::string& line) {
  const size_t end = in.find('\n', pos);
  if (end == std::string::npos) return false;
  line.assign(in, pos, end - pos);
  pos = end + 1;
  return true;
}
}  // namespace

bool encode(const std::string& feed, const std::vector<std::string>& history, const int row, std::string& out) {
  out.clear();
  if (feed.empty() && history.empty()) return false;
  if (!storable(feed)) return false;
  // Too deep: the root stays at the bottom and the oldest steps above it go.
  const size_t skipFrom = history.size() > kMaxDepth ? 1 : 0;
  const size_t skipTo = history.size() > kMaxDepth ? history.size() - (kMaxDepth - 1) : 0;
  for (size_t i = 0; i < history.size(); ++i) {
    if (i >= skipFrom && i < skipTo) continue;
    if (!storable(history[i])) return false;
  }

  char number[12];
  snprintf(number, sizeof(number), "%d", row < 0 ? 0 : (row > UINT16_MAX ? UINT16_MAX : row));
  out = kMagic;
  out += '\n';
  out += number;
  out += '\n';
  out += feed;
  out += '\n';
  for (size_t i = 0; i < history.size(); ++i) {
    if (i >= skipFrom && i < skipTo) continue;
    out += history[i];
    out += '\n';
  }
  return true;
}

bool decode(const std::string& in, OpdsSavedPosition& out) {
  out = {};
  if (in.size() > kMaxFileBytes) return false;
  size_t pos = 0;
  std::string line;
  if (!nextLine(in, pos, line) || line != kMagic) return false;

  if (!nextLine(in, pos, line) || line.empty() || line.size() > 5) return false;
  unsigned row = 0;
  for (const char c : line) {
    if (c < '0' || c > '9') return false;
    row = row * 10 + static_cast<unsigned>(c - '0');
  }
  if (row > UINT16_MAX) return false;

  OpdsSavedPosition result;
  result.row = static_cast<uint16_t>(row);
  if (!nextLine(in, pos, result.feed) || !storable(result.feed)) return false;
  while (pos < in.size()) {
    if (result.history.size() >= kMaxDepth || !nextLine(in, pos, line) || !storable(line)) return false;
    result.history.push_back(line);
  }
  if (result.feed.empty() && result.history.empty()) return false;
  out = std::move(result);
  return true;
}
}  // namespace OpdsPosition
