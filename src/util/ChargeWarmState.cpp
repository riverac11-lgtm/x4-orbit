#include "ChargeWarmState.h"

#include <cstring>

namespace ChargeWarm {

namespace {
constexpr uint32_t kMagic = 0x0C4A5701;  // bumped when Stored changes

// Fixed part of the file; the book's path follows it.
struct Stored {
  uint32_t magic;
  Frame frame;
  uint32_t sectionHash;
  int16_t spine;
  uint16_t page;
  int16_t endSpine;
  uint16_t pathLength;
};
}  // namespace

State fromReader(const std::string& bookPath, const Frame& frame, const uint32_t sectionHash, const int spine,
                 const int page, const int spineCount) {
  State s;
  s.frame = frame;
  s.sectionHash = sectionHash;
  s.bookPath = bookPath;
  if (spine < 0 || spine >= spineCount || spineCount > INT16_MAX || page < 0 || page > UINT16_MAX) {
    return s;  // spine == endSpine: nothing pending
  }
  s.spine = static_cast<int16_t>(spine);
  s.page = static_cast<uint16_t>(page);
  const int end = spine + 1 + kSpinesAhead;
  s.endSpine = static_cast<int16_t>(end < spineCount ? end : spineCount);
  return s;
}

bool hashFromSectionPath(const std::string_view path, uint32_t& hash) {
  const size_t slash = path.rfind('/');
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const size_t underscore = name.find('_');
  // "<digits>_<8 hex>.bin"
  if (underscore == 0 || underscore == std::string_view::npos || name.size() != underscore + 1 + 8 + 4 ||
      name.substr(name.size() - 4) != ".bin") {
    return false;
  }
  for (size_t i = 0; i < underscore; ++i) {
    if (name[i] < '0' || name[i] > '9') return false;
  }
  uint32_t value = 0;
  for (size_t i = underscore + 1; i < underscore + 9; ++i) {
    const char c = name[i];
    uint32_t digit;
    if (c >= '0' && c <= '9') {
      digit = static_cast<uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      digit = static_cast<uint32_t>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      digit = static_cast<uint32_t>(c - 'A' + 10);
    } else {
      return false;
    }
    value = (value << 4) | digit;
  }
  hash = value;
  return true;
}

bool inCompleted(const std::string_view bookPath) {
  constexpr std::string_view kDir = "/COMPLETED";
  return bookPath.substr(0, kDir.size()) == kDir && (bookPath.size() == kDir.size() || bookPath[kDir.size()] == '/');
}

bool frameUsable(const Frame& f) {
  if (f.panelWidth < 64 || f.panelHeight < 64 || f.panelWidth > 2048 || f.panelHeight > 2048) return false;
  if (f.panelWidthBytes < (f.panelWidth + 7) / 8 || f.panelWidthBytes > f.panelWidth / 8 + 8) return false;
  if (f.orientation > 3 || f.grayscale > 1) return false;
  const int longSide = f.panelWidth > f.panelHeight ? f.panelWidth : f.panelHeight;
  return f.left >= 0 && f.top >= 0 && f.left < longSide && f.top < longSide && f.viewportHeight >= 0 &&
         f.viewportHeight <= longSide;
}

bool pending(const State& s) {
  return s.spine >= 0 && s.spine < s.endSpine && !s.bookPath.empty() && !inCompleted(s.bookPath) &&
         frameUsable(s.frame);
}

void nextSpine(State& s) {
  if (s.spine < s.endSpine) ++s.spine;
  s.page = 0;
}

void encode(const State& s, std::string& out) {
  const size_t length = s.bookPath.size() < kMaxPathLength ? s.bookPath.size() : kMaxPathLength;
  const Stored stored{kMagic, s.frame, s.sectionHash, s.spine, s.page, s.endSpine, static_cast<uint16_t>(length)};
  out.assign(reinterpret_cast<const char*>(&stored), sizeof(stored));
  out.append(s.bookPath.data(), length);
}

bool decode(const std::string_view data, State& out) {
  out = State{};
  Stored stored{};
  if (data.size() < sizeof(stored)) return false;
  memcpy(&stored, data.data(), sizeof(stored));
  if (stored.magic != kMagic || stored.pathLength == 0 || stored.pathLength > kMaxPathLength ||
      data.size() != sizeof(stored) + stored.pathLength || stored.spine < 0 || stored.endSpine < 0 ||
      stored.spine > stored.endSpine) {
    return false;
  }
  out.frame = stored.frame;
  out.sectionHash = stored.sectionHash;
  out.spine = stored.spine;
  out.page = stored.page;
  out.endSpine = stored.endSpine;
  out.bookPath.assign(data.data() + sizeof(stored), stored.pathLength);
  return true;
}

}  // namespace ChargeWarm
