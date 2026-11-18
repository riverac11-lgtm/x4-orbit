#include "ReadingInsights.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ReadingInsights {

namespace {

// FNV-1a over the count, the book and the sessions: enough to tell a torn write or another file.
uint32_t checksum(const SessionLog& log) {
  uint32_t h = 2166136261u;
  const auto mix = [&h](const uint8_t* p, const size_t n) {
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
  };
  mix(&log.count, 1);
  mix(reinterpret_cast<const uint8_t*>(log.lastDocId), sizeof(log.lastDocId));
  mix(reinterpret_cast<const uint8_t*>(log.sessions), sizeof(log.sessions));
  return h;
}

bool sameText(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const auto fold = [](const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    if (fold(a[i]) != fold(b[i])) return false;
  }
  return true;
}

}  // namespace

// ---- The recent sessions ---------------------------------------------------------------------

bool add(SessionLog& log, const char* docId, const uint32_t seconds, const uint32_t pages) {
  if (seconds < kMinSeconds) return false;
  if (log.count > kCapacity) log.count = kCapacity;
  if (log.count == kCapacity) {
    std::copy(log.sessions + 1, log.sessions + kCapacity, log.sessions);
    --log.count;
  }
  log.sessions[log.count++] = Session{seconds, pages};
  std::snprintf(log.lastDocId, sizeof(log.lastDocId), "%s", docId ? docId : "");
  return true;
}

void seal(SessionLog& log) {
  log.magic = kMagic;
  log.check = checksum(log);
}

bool valid(const SessionLog& log) {
  return log.magic == kMagic && log.count <= kCapacity && log.check == checksum(log) &&
         std::memchr(log.lastDocId, '\0', sizeof(log.lastDocId)) != nullptr;
}

const Session* newest(const SessionLog& log) { return log.count == 0 ? nullptr : &log.sessions[log.count - 1]; }

uint32_t averageSeconds(const SessionLog& log) {
  if (log.count == 0) return 0;
  uint64_t sum = 0;
  for (uint8_t i = 0; i < log.count; ++i) sum += log.sessions[i].seconds;
  return static_cast<uint32_t>(sum / log.count);
}

uint32_t longestSeconds(const SessionLog& log) {
  uint32_t longest = 0;
  for (uint8_t i = 0; i < log.count; ++i) longest = std::max(longest, log.sessions[i].seconds);
  return longest;
}

// ---- The current book: time left ------------------------------------------------------------

TimeLeft timeLeft(const uint32_t bookSeconds, const uint8_t progress, const uint32_t averageSessionSeconds) {
  TimeLeft out;
  if (progress >= 100 || progress < kEstimateMinPercent || bookSeconds < kEstimateMinSeconds) return out;
  out.known = true;
  const uint64_t left = static_cast<uint64_t>(bookSeconds) * (100u - progress) / progress;
  out.seconds = static_cast<uint32_t>(std::min<uint64_t>(left, UINT32_MAX));
  if (averageSessionSeconds > 0) {
    const uint64_t sessions = (left + averageSessionSeconds - 1) / averageSessionSeconds;
    out.sessions = static_cast<uint16_t>(std::clamp<uint64_t>(sessions, 1, 9999));
  }
  return out;
}

// ---- Milestones -----------------------------------------------------------------------------

uint32_t nextThreshold(const Kind kind, const uint32_t value) {
  if (kind == Kind::Books) {
    for (const uint32_t t : {5u, 10u, 15u}) {
      if (value < t) return t;
    }
  }
  // Steps of 1, 2.5 and 5 in each decade, from 10 h, 1000 pages, 25 books.
  static constexpr uint8_t kSteps[3] = {10, 25, 50};
  uint64_t decade = kind == Kind::Pages ? 1000 : 10;
  size_t step = kind == Kind::Books ? 1 : 0;
  for (;;) {
    for (; step < 3; ++step) {
      const uint64_t t = decade * kSteps[step] / 10;
      if (t > value) return static_cast<uint32_t>(std::min<uint64_t>(t, UINT32_MAX));
    }
    step = 0;
    decade *= 10;
  }
}

Milestone milestone(const Kind kind, const uint32_t amount) {
  Milestone m;
  m.kind = kind;
  m.value = kind == Kind::Hours ? amount / 3600 : amount;
  m.next = nextThreshold(kind, 0);
  while (m.next <= m.value && m.next != UINT32_MAX) {
    m.previous = m.next;
    m.next = nextThreshold(kind, m.next);
  }
  // Hours count in seconds below, so "to go" and the bar move with every minute read.
  const uint64_t unit = kind == Kind::Hours ? 3600 : 1;
  const uint64_t done = kind == Kind::Hours ? amount : m.value;
  const uint64_t from = m.previous * unit;
  const uint64_t to = m.next * unit;
  m.toGo = static_cast<uint32_t>(std::min<uint64_t>(to - done, UINT32_MAX));
  m.permille = static_cast<int>((done - from) * 1000 / (to - from));
  return m;
}

size_t closest(const Milestone* milestones, const size_t count) {
  size_t best = 0;
  for (size_t i = 1; i < count; ++i) {
    if (milestones[i].permille > milestones[best].permille) best = i;
  }
  return best;
}

// ---- Top books --------------------------------------------------------------------------------

void mergeDuplicates(std::vector<BookReadingStats>& books) {
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i].title.empty()) continue;
    for (size_t j = i + 1; j < books.size();) {
      if (sameText(books[i].title, books[j].title) && sameText(books[i].author, books[j].author)) {
        books[i].totalSeconds += books[j].totalSeconds;
        books[i].pagesTurned += books[j].pagesTurned;
        books[i].sessions += books[j].sessions;
        books.erase(books.begin() + static_cast<std::ptrdiff_t>(j));
      } else {
        ++j;
      }
    }
  }
  std::stable_sort(books.begin(), books.end(), [](const BookReadingStats& a, const BookReadingStats& b) {
    return a.totalSeconds > b.totalSeconds;
  });
}

// ---- Formatting --------------------------------------------------------------------------------

void formatDuration(char* out, const size_t size, const uint32_t seconds, const char* hm, const char* h,
                    const char* m) {
  const uint32_t minutes = seconds / 60;
  const int hours = static_cast<int>(minutes / 60);
  const int rest = static_cast<int>(minutes % 60);
  if (hours == 0) {
    std::snprintf(out, size, m, rest);
  } else if (rest == 0) {
    std::snprintf(out, size, h, hours);
  } else {
    std::snprintf(out, size, hm, hours, rest);
  }
}

void groupThousands(char* out, const size_t size, const uint32_t value) {
  char digits[12];
  const int n = std::snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
  char grouped[16];
  int g = 0;
  for (int i = 0; i < n; ++i) {
    if (i > 0 && (n - i) % 3 == 0) grouped[g++] = ',';
    grouped[g++] = digits[i];
  }
  grouped[g] = '\0';
  std::snprintf(out, size, "%s", grouped);
}

}  // namespace ReadingInsights
