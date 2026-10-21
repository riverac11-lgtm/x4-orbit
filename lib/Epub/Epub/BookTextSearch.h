#pragma once

#include <Print.h>
#include <expat.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Full-text search over a book, one chapter (spine item) at a time.
//
// A chapter's XHTML is streamed through expat (Epub::readItemContentsToStream),
// so memory stays small regardless of chapter size. Matching is case- and
// accent-insensitive, treats any run of whitespace as one space, and runs
// across inline markup ("wor<i>ld</i>" matches "world"); block boundaries
// (paragraphs, headings, list items, <br>) count as a space so words never
// merge across them.
//
// Hit positions use the reader's "visible text offset": the count of UTF-8
// codepoints of character data inside <body>, outside head/style/script/title/
// rp, with known HTML entities expanded — exactly what ChapterHtmlSlimParser
// counts while laying out a chapter (and what ChapterXPathResolver mirrors).
// Section::getPageForVisibleTextOffset() therefore maps a hit to its page, and
// a laid-out word's SourceRange tells whether it is part of the match.
namespace booksearch {

// Folds one codepoint for comparison: lowercase, accents stripped for Latin
// letters, typographic quotes/dashes to ASCII. Returns ' ' for whitespace and
// 0 for characters that are ignored entirely (soft hyphen, zero-width marks).
uint32_t foldCodepoint(uint32_t cp);

// The query in folded form: trimmed, inner whitespace collapsed to one space.
// Empty when the query has no searchable characters.
std::vector<uint32_t> foldQuery(const std::string& utf8);

struct Hit {
  uint32_t visibleOffset = 0;  // first matched codepoint
  uint32_t visibleEnd = 0;     // one past the last matched codepoint
  std::string snippet;         // UTF-8 context around the match, whitespace collapsed
};

class ChapterSearcher final : public Print {
 public:
  // `needle` comes from foldQuery() and must be non-empty. Collection stops
  // after `maxHits`; contextChars codepoints of context are kept each side.
  ChapterSearcher(const std::vector<uint32_t>& needle, size_t maxHits, size_t contextChars = 28);
  ~ChapterSearcher() override;
  ChapterSearcher(const ChapterSearcher&) = delete;
  ChapterSearcher& operator=(const ChapterSearcher&) = delete;

  bool ok() const { return parser != nullptr && parseOk; }

  // Polled once per streamed chunk; returning true abandons the chapter
  // (aborted() then reports it). Lets the UI cancel a long chapter mid-way.
  using AbortProbe = bool (*)();
  void setAbortProbe(AbortProbe probe) { abortProbe = probe; }
  bool aborted() const { return wasAborted; }
  // Flushes the parser and completes any snippet still collecting context.
  bool finish();

  size_t write(uint8_t c) override { return write(&c, 1); }
  // Returns 0 once nothing more is wanted (cap reached, aborted, or a parse
  // error), so a reader streaming with allowEarlyStop stops inflating.
  size_t write(const uint8_t* buffer, size_t size) override;

  const std::vector<Hit>& hits() const { return found; }
  std::vector<Hit>&& takeHits() { return std::move(found); }
  bool full() const { return found.size() >= maxHits; }

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  static void XMLCALL defaultHandler(void* userData, const XML_Char* s, int len);

  void onStart(const char* name);
  void onEnd(const char* name);
  void onText(const char* s, int len);
  // One folded unit of the comparison stream (srcOffset: its visible offset).
  void emit(uint32_t folded, uint32_t original, uint32_t srcOffset);
  void blockBoundary();
  void appendContext(uint32_t original);

  XML_Parser parser = nullptr;
  bool parseOk = true;
  bool stopped = false;
  AbortProbe abortProbe = nullptr;
  bool wasAborted = false;

  // Visible-offset counting, mirroring ChapterHtmlSlimParser.
  bool insideBody = false;
  int depth = 0;
  int bodyDepth = -1;
  int nonVisibleDepth = 0;
  uint32_t visibleOffset = 0;

  // Matching (KMP over the folded stream).
  std::vector<uint32_t> needle;
  std::vector<int> failure;
  int matched = 0;
  bool lastWasSpace = true;  // drops leading spaces and collapses runs
  // Ring of source offsets of the last needle.size() emitted units.
  std::vector<uint32_t> offsetRing;
  size_t emittedCount = 0;

  // Snippets: a ring of recent original codepoints (before-context), and hits
  // still collecting after-context.
  size_t contextChars;
  std::vector<uint32_t> recent;  // ring, capacity contextChars + needle.size()
  size_t recentCount = 0;
  struct Pending {
    size_t hitIndex;
    size_t remaining;
  };
  std::vector<Pending> pending;

  size_t maxHits;
  std::vector<Hit> found;
};

}  // namespace booksearch
