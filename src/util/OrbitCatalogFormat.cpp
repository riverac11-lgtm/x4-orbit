#include "OrbitCatalogFormat.h"

#include <Epub/BookTextSearch.h>
#include <Utf8.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace OrbitCatalog {

namespace {
constexpr uint32_t kFirstRealTime = 1577836800;  // 2020-01-01: HalClock's own "set" threshold
}

uint32_t lastUpdated(const Header& header) {
  const uint32_t t = std::max(header.fullTime, header.updateTime);
  return t >= kFirstRealTime ? t : 0;
}

bool formatStamp(const uint32_t t, char* out, const size_t size) {
  if (size == 0) return false;
  out[0] = '\0';
  if (t < kFirstRealTime) return false;
  const time_t when = static_cast<time_t>(t);
  struct tm tm{};
  if (!localtime_r(&when, &tm)) return false;
  snprintf(out, size, "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  return true;
}

namespace {
uint32_t parseUint(std::string_view s) {
  uint32_t v = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') break;
    v = v * 10 + static_cast<uint32_t>(c - '0');
  }
  return v;
}

// Appends a field: tabs and line breaks become spaces, the rest is copied as is.
void appendField(std::string& out, std::string_view value) {
  out += '\t';
  for (const char c : value) out += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
}

void appendNumber(std::string& out, const uint32_t value) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
  out += buf;
}

// Splits `line` at tabs into at most `max` views. Returns the number of fields.
size_t splitTabs(std::string_view line, std::string_view* fields, const size_t max) {
  size_t n = 0;
  size_t start = 0;
  while (n < max) {
    const size_t tab = line.find('\t', start);
    if (tab == std::string_view::npos) {
      fields[n++] = line.substr(start);
      break;
    }
    fields[n++] = line.substr(start, tab - start);
    start = tab + 1;
  }
  return n;
}

// The folded units of a string, through the callback; whitespace runs collapsed to one ' ' and
// leading whitespace dropped. Trailing whitespace may be emitted once (callers trim it).
template <typename Emit>
void foldUnits(std::string_view utf8, Emit&& emit) {
  const auto* p = reinterpret_cast<const unsigned char*>(utf8.data());
  const auto* end = p + utf8.size();
  bool lastSpace = true;
  while (p < end) {
    const unsigned char* before = p;
    const uint32_t cp = utf8NextCodepoint(&p);
    if (p <= before || p > end) break;  // malformed tail: stop rather than read past the view
    const uint32_t folded = booksearch::foldCodepoint(cp);
    if (folded == 0) continue;
    if (folded == ' ') {
      if (lastSpace) continue;
      lastSpace = true;
    } else {
      lastSpace = false;
    }
    if (!emit(folded)) return;
  }
}

bool startsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }
bool has(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }
}  // namespace

uint16_t RecordView::gen() const { return static_cast<uint16_t>(parseUint(f[kGen])); }
uint32_t RecordView::size() const { return parseUint(f[kSize]); }

bool splitRecord(std::string_view line, RecordView& out) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  if (splitTabs(line, out.f, kFieldCount) != kFieldCount) return false;
  return out.f[kTag] == "B" && !out.f[kId].empty() && !out.f[kUrl].empty();
}

void toBook(const RecordView& view, Book& out) {
  out.id.assign(view[kId]);
  out.title.assign(view[kTitle]);
  out.author.assign(view[kAuthor]);
  out.series.assign(view[kSeries]);
  out.seriesIndex.assign(view[kSeriesIndex]);
  out.format.assign(view[kFormat]);
  out.updated.assign(view[kUpdated]);
  out.url.assign(view[kUrl]);
  out.cover.assign(view[kCover]);
  out.size = view.size();
  out.gen = view.gen();
}

bool appendRecord(const Book& book, std::string& out) {
  if (book.url.empty() || book.url.size() > kMaxUrl || book.id.empty()) return false;
  out += 'B';
  out += '\t';
  appendNumber(out, book.gen);
  appendField(out, book.id);
  appendField(out, book.title);
  appendField(out, book.author);
  appendField(out, book.series);
  appendField(out, book.seriesIndex);
  appendField(out, book.format);
  out += '\t';
  appendNumber(out, book.size);
  appendField(out, book.updated);
  appendField(out, book.url);
  appendField(out, book.cover.size() > kMaxUrl ? std::string_view{} : std::string_view{book.cover});
  out += '\n';
  return true;
}

void appendHeader(const Header& header, const char* kind, std::string& out) {
  char buf[96];
  snprintf(buf, sizeof(buf), "%s\t%d\t%lu\t%u\t%lu\t%lu\n", kind, kVersion, static_cast<unsigned long>(header.count),
           static_cast<unsigned>(header.gen), static_cast<unsigned long>(header.fullTime),
           static_cast<unsigned long>(header.updateTime));
  out += buf;
}

bool parseHeader(std::string_view line, const char* kind, Header& out) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  std::string_view f[6];
  if (splitTabs(line, f, 6) != 6 || f[0] != kind || parseUint(f[1]) != static_cast<uint32_t>(kVersion) ||
      f[2].empty() || f[2][0] < '0' || f[2][0] > '9') {
    return false;
  }
  out.count = parseUint(f[2]);
  out.gen = static_cast<uint16_t>(parseUint(f[3]));
  out.fullTime = parseUint(f[4]);
  out.updateTime = parseUint(f[5]);
  return true;
}

uint32_t idHash(std::string_view id) {
  uint32_t h = 2166136261u;
  for (const unsigned char c : id) h = (h ^ c) * 16777619u;
  return h;
}

int32_t seriesOrderKey(std::string_view index) {
  while (!index.empty() && index.front() == ' ') index.remove_prefix(1);
  if (index.empty() || index[0] < '0' || index[0] > '9') return INT32_MAX;
  int64_t whole = 0;
  size_t i = 0;
  for (; i < index.size() && index[i] >= '0' && index[i] <= '9'; ++i) {
    whole = whole * 10 + (index[i] - '0');
    if (whole > 1000000) return INT32_MAX - 1;
  }
  int32_t fraction = 0;
  if (i < index.size() && (index[i] == '.' || index[i] == ',')) {
    int32_t scale = 100;
    for (++i; i < index.size() && index[i] >= '0' && index[i] <= '9' && scale > 0; ++i, scale /= 10) {
      fraction += (index[i] - '0') * scale;
    }
  }
  return static_cast<int32_t>(whole * 1000 + fraction);
}

bool seriesFromSummary(std::string_view summary, std::string& name, std::string& index) {
  // calibre: "SERIES: Name [2]" (any case); some catalogs: "Series: Name #2".
  size_t at = std::string_view::npos;
  for (size_t i = 0; i + 7 <= summary.size(); ++i) {
    if ((summary[i] == 'S' || summary[i] == 's') &&
        (summary.substr(i + 1, 6) == "ERIES:" || summary.substr(i + 1, 6) == "eries:")) {
      at = i + 7;
      break;
    }
  }
  if (at == std::string_view::npos) return false;
  std::string_view rest = summary.substr(at);
  const size_t lineEnd = rest.find_first_of("\n\r<");
  if (lineEnd != std::string_view::npos) rest = rest.substr(0, lineEnd);
  while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
  while (!rest.empty() && rest.back() == ' ') rest.remove_suffix(1);
  std::string_view number;
  if (!rest.empty() && rest.back() == ']') {
    const size_t open = rest.rfind('[');
    if (open != std::string_view::npos) {
      number = rest.substr(open + 1, rest.size() - open - 2);
      rest = rest.substr(0, open);
    }
  } else if (const size_t hash = rest.rfind('#'); hash != std::string_view::npos) {
    number = rest.substr(hash + 1);
    rest = rest.substr(0, hash);
  }
  while (!rest.empty() && rest.back() == ' ') rest.remove_suffix(1);
  if (rest.empty() || rest.size() > 200) return false;
  name.assign(rest);
  index.assign(number);
  return true;
}

const char* extensionFor(std::string_view formatKey) {
  if (formatKey == "kepub") return ".kepub.epub";
  // The key is the extension for every other format the reader opens; anything else is EPUB.
  static constexpr const char* kSame[] = {".txt", ".md", ".xtc", ".xtch"};
  for (const char* ext : kSame) {
    if (formatKey == ext + 1) return ext;
  }
  return ".epub";
}

const char* mimeFor(std::string_view formatKey) {
  if (formatKey == "txt") return "text/plain";
  if (formatKey == "md") return "text/markdown";
  if (formatKey == "xtc") return "application/vnd.xteink.xtc";
  if (formatKey == "xtch") return "application/vnd.xteink.xtch";
  return "application/epub+zip";
}

void fold(std::string_view utf8, std::string& out) {
  out.clear();
  foldUnits(utf8, [&out](const uint32_t unit) {
    utf8AppendCodepoint(out, unit);
    return true;
  });
  if (!out.empty() && out.back() == ' ') out.pop_back();
}

bool containsFolded(std::string_view text, const std::string& foldedNeedle) {
  if (foldedNeedle.empty()) return true;
  // A title or an author: short, so folding it whole costs one small allocation.
  std::string folded;
  fold(text, folded);
  return folded.find(foldedNeedle) != std::string::npos;
}

void sortKey(std::string_view name, char (&out)[kSortKeyLength]) {
  memset(out, 0, sizeof(out));
  size_t n = 0;
  foldUnits(name, [&](const uint32_t unit) {
    out[n++] = unit < 0x80 ? static_cast<char>(unit) : '~';
    return n < kSortKeyLength;
  });
}

bool matchesRole(std::string_view title, const RootRole role) {
  std::string t;
  fold(title, t);
  switch (role) {
    case RootRole::AllBooks:  // "All Books", "All titles", "All", "Books", "By title"
      return startsWith(t, "all") || t == "books" || t == "titles" || t == "by title";
    case RootRole::Recent:  // "Recent Books", "Recently added", "Latest additions", "Newest", "New"
      return has(t, "recent") || has(t, "latest") || startsWith(t, "new");
    case RootRole::Collections:  // "Collections", "Shelves", "My shelf"
      return has(t, "collection") || has(t, "shel");
  }
  return false;
}

bool isSendCollection(std::string_view title) {
  // "Send to X4", "send-to-x4", "SEND TO X4 " ...: ASCII letters and digits only, any case.
  char compact[9];
  size_t n = 0;
  for (char c : title) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if ((c < 'a' || c > 'z') && (c < '0' || c > '9')) continue;
    if (n == sizeof(compact)) return false;
    compact[n++] = c;
  }
  return std::string_view(compact, n) == "sendtox4";
}

MergeStop::Verdict MergeStop::offer(std::string_view id) {
  const uint32_t h = idHash(id);
  if (std::binary_search(known.begin(), known.end(), h)) {
    if (++knownRun >= kStopAfterKnown) return Verdict::Stop;
    return Verdict::Known;
  }
  if (std::find(seenNew.begin(), seenNew.end(), h) != seenNew.end()) return Verdict::Known;
  knownRun = 0;
  if (added >= kMaxNew) return Verdict::TooMany;
  seenNew.push_back(h);
  ++added;
  return Verdict::New;
}

}  // namespace OrbitCatalog
