#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The saved OPDS catalog (Online Library offline view) and the "waiting to download" list: their
// line format and the pure rules around them. No SD card, no Arduino (host tests: test/orbit_catalog).
//
// One file per server, plain text, one line per book, so a reader can stream it and never holds
// the catalog in RAM:
//   ORBITCAT\t1\t<count>\t<gen>\t<fullBuildTime>\t<updateTime>          (header, first line)
//   B\t<gen>\t<id>\t<title>\t<author>\t<series>\t<index>\t<format>\t<size>\t<updated>\t<url>\t<cover>
// <gen> is the update that added the book (1 = the full build; each update adds one), which is
// what "New since last update" lists. Times are epoch seconds, 0 when the clock was unknown.
// Fields never contain a tab or a line break (replaced by spaces when written). The waiting list
// uses the same book lines with a header of its own kind.
namespace OrbitCatalog {

constexpr int kVersion = 1;
// A line read back longer than this is skipped as damaged (a URL is capped well below it).
constexpr size_t kMaxLine = 3072;
constexpr size_t kMaxUrl = 2048;

enum Field : uint8_t {
  kTag,
  kGen,
  kId,
  kTitle,
  kAuthor,
  kSeries,
  kSeriesIndex,
  kFormat,
  kSize,
  kUpdated,
  kUrl,
  kCover,
  kFieldCount
};

// A book line split in place: every view points into the caller's line buffer.
struct RecordView {
  std::string_view f[kFieldCount];
  std::string_view operator[](const Field field) const { return f[field]; }
  uint16_t gen() const;
  uint32_t size() const;
};

struct Book {
  std::string id;
  std::string title;
  std::string author;
  std::string series;
  std::string seriesIndex;
  std::string format;  // OpdsAcquisitionLink::formatKey: epub, kepub, txt, md, xtc, xtch
  std::string updated;
  std::string url;    // absolute
  std::string cover;  // absolute, may be empty
  uint32_t size = 0;
  uint16_t gen = 0;
};

struct Header {
  uint32_t count = 0;
  uint16_t gen = 0;
  uint32_t fullTime = 0;
  uint32_t updateTime = 0;
};

// `line` without its line break. False for anything that is not a well-formed book line.
bool splitRecord(std::string_view line, RecordView& out);
void toBook(const RecordView& view, Book& out);
// Appends the book's line (with its '\n'). False, appending nothing, when it cannot be fetched
// later (no URL, or one too long).
bool appendRecord(const Book& book, std::string& out);

void appendHeader(const Header& header, const char* kind, std::string& out);
bool parseHeader(std::string_view line, const char* kind, Header& out);
constexpr char kCatalogKind[] = "ORBITCAT";
constexpr char kWaitingKind[] = "ORBITWAIT";

// FNV-1a of a book's id: what the catalog and the waiting list compare.
uint32_t idHash(std::string_view id);

// The series number as a sortable integer: "2" -> 2000, "2.5" -> 2500, "" or junk -> INT32_MAX
// (unnumbered books last). Integer math only (the firmware has no float on its hot paths).
int32_t seriesOrderKey(std::string_view index);
// calibre's content server writes the series into the entry text: "SERIES: Foundation [2]". Fills
// name and index when the summary carries that; false otherwise.
bool seriesFromSummary(std::string_view summary, std::string& name, std::string& index);

// The file extension a downloaded book of this format is saved with, and its MIME type.
const char* extensionFor(std::string_view formatKey);
const char* mimeFor(std::string_view formatKey);

// Case- and accent-insensitive comparison text: the string folded codepoint by codepoint
// (booksearch::foldCodepoint), as UTF-8, whitespace collapsed to one space and trimmed.
void fold(std::string_view utf8, std::string& out);
// True when the folded `needle` is found in `text` (folded on the fly). An empty needle matches.
bool containsFolded(std::string_view text, const std::string& foldedNeedle);
// A short ASCII sort key for a name (folded, non-ASCII as '~'), for ordering lists of names
// without holding the names: `out` gets up to kSortKeyLength bytes, zero padded.
constexpr size_t kSortKeyLength = 12;
void sortKey(std::string_view name, char (&out)[kSortKeyLength]);

// Which entry of the server's root feed is which, by title, tolerant of case, accents and wording.
enum class RootRole : uint8_t { AllBooks, Recent, Collections };
bool matchesRole(std::string_view title, RootRole role);
// The collection whose books are delivered while charging: "Send to X4", any case or spacing.
bool isSendCollection(std::string_view title);

// The stop rule of an incremental update. The "Recent Books" feed is walked from the newest; each
// book's id is offered here. Unknown ids are new. Walking stops once kStopAfterKnown known ids
// have been seen in a row (one old book that was edited and moved up does not end the walk
// early), or after kMaxNew new ones (then a full rebuild is the better answer).
class MergeStop {
 public:
  static constexpr int kStopAfterKnown = 3;
  static constexpr int kMaxNew = 400;
  enum class Verdict : uint8_t { New, Known, Stop, TooMany };
  // `knownSorted`: idHash() of every book in the catalog, sorted ascending. Not copied.
  explicit MergeStop(const std::vector<uint32_t>& knownSorted) : known(knownSorted) {}
  Verdict offer(std::string_view id);
  int newCount() const { return added; }

 private:
  const std::vector<uint32_t>& known;
  std::vector<uint32_t> seenNew;  // ids already counted new in this walk (a feed may repeat one)
  int knownRun = 0;
  int added = 0;
};

}  // namespace OrbitCatalog
