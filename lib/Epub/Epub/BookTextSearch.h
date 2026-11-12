#pragma once

#include <Print.h>
#include <SaxParser/SaxParser.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Full-text search over a book, one chapter (spine item) at a time.
//
// A chapter's XHTML is streamed through the SAX parser (Epub::readItemContentsToStream), so
// memory stays small regardless of chapter size. Matching is case- and accent-insensitive,
// treats any run of whitespace as one space, and runs across inline markup ("wor<i>ld</i>"
// matches "world"); block boundaries (paragraphs, headings, list items, <br>) count as a
// space so words never merge across them.
//
// Hit positions use the reader's visible-text offset (see VisibleText.h): the number of
// non-whitespace bytes of character data inside <body>, outside head/script/style, entity
// references as their expansion -- exactly what ChapterHtmlSlimParser counts while laying
// out a chapter. Section::getPageForVisibleTextOffset() therefore maps a hit to its page.
namespace booksearch {

// Folds one codepoint for comparison: lowercase, accents stripped for Latin letters,
// typographic quotes/dashes to ASCII. Returns ' ' for whitespace and 0 for characters that
// are ignored entirely (soft hyphen, zero-width marks).
uint32_t foldCodepoint(uint32_t cp);

// Search options. Both default to off: the plain search finds the text anywhere, in any case.
//
// wholeWord: a match counts only when it is not part of a longer word. The rule: if the
//   query starts with a word character, the character just before the match must not be one,
//   and likewise for its end and the character just after. A word character (isWordChar) is
//   an ASCII letter or digit, or any character from U+00C0 up that is not punctuation, a
//   symbol or CJK: so accented Latin, Greek, Cyrillic, Hebrew, Arabic... letters all count.
//   Space, block boundaries, the start and end of a chapter, apostrophes, hyphens and
//   underscores do not ("don" is found in "don't", "well" in "well-known"). CJK ideographs
//   and kana are written without spaces, so each stands as its own word and the option
//   changes nothing for them. Ignored characters (soft hyphen, zero-width marks) are skipped
//   before the rule applies, so a soft-hyphenated word is still one word.
// matchCase: capitals must match ("Bill" does not find "bill"). Accents are still ignored.
//   Case is known for the same letters foldCodepoint() lowercases: Latin (ASCII, Latin-1,
//   Extended-A), Greek and Cyrillic capitals.
struct Options {
  bool wholeWord = false;
  bool matchCase = false;
};

// See Options::wholeWord. Takes a codepoint or a matchKey().
bool isWordChar(uint32_t cp);

// The unit the matcher compares: foldCodepoint(), with CASE_BIT set for a capital letter
// when matchCase is on. ' ' and 0 as for foldCodepoint().
inline constexpr uint32_t CASE_BIT = 0x80000000u;
uint32_t matchKey(uint32_t cp, bool matchCase);

// The query in folded form (matchKey units): trimmed, inner whitespace collapsed to one
// space. Empty when the query has no searchable characters.
std::vector<uint32_t> foldQuery(const std::string& utf8, bool matchCase = false);

struct Hit {
  uint32_t visibleOffset = 0;  // visible offset of the first matched character
  uint32_t visibleEnd = 0;     // one past the last matched byte
  std::string snippet;         // UTF-8 context around the match, whitespace collapsed
};

class ChapterSearcher final : public Print {
 public:
  // `needle` comes from foldQuery() -- called with the same matchCase as `options` -- and must
  // be non-empty. Collection stops after `maxHits`; contextChars codepoints of context are
  // kept each side.
  ChapterSearcher(const std::vector<uint32_t>& needle, size_t maxHits, size_t contextChars = 28,
                  Options options = {});
  ~ChapterSearcher() override;
  ChapterSearcher(const ChapterSearcher&) = delete;
  ChapterSearcher& operator=(const ChapterSearcher&) = delete;

  bool ok() const { return parserOk; }

  // Polled once per streamed chunk; returning true abandons the chapter (aborted() then
  // reports it). Lets the UI cancel a long chapter mid-way.
  using AbortProbe = bool (*)();
  void setAbortProbe(AbortProbe probe) { abortProbe = probe; }
  bool aborted() const { return wasAborted; }

  // Pass to Epub::readItemContentsToStream as its stop flag: set once nothing more is wanted
  // (cap reached, aborted, parse error), so the zip reader stops inflating.
  const bool* stopFlag() const { return &stopped; }

  // Flushes the parser and completes any snippet still collecting context.
  bool finish();

  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* buffer, size_t size) override;

  const std::vector<Hit>& hits() const { return found; }
  std::vector<Hit>&& takeHits() { return std::move(found); }
  bool full() const { return found.size() >= maxHits; }
  // Visible bytes counted so far (the whole chapter after an uninterrupted finish()).
  uint32_t countedBytes() const { return visibleOffset; }

 private:
  static void startElement(void* userData, const char* name, const char** atts);
  static void endElement(void* userData, const char* name);
  static void characterData(void* userData, const char* s, int len);
  static void defaultHandler(void* userData, const char* s, int len);

  void onStart(const char* name);
  void onEnd(const char* name);
  void onText(const char* s, int len);
  // One folded unit of the comparison stream; [srcOffset, srcEnd) is its visible-offset span.
  void emit(uint32_t folded, uint32_t original, uint32_t srcOffset, uint32_t srcEnd);
  void blockBoundary();
  void appendContext(uint32_t original);
  void stopParsing();
  void acceptHit(Hit&& hit);

  SaxParser parser;
  bool parserOk = false;
  bool parseFailed = false;
  bool stopped = false;
  AbortProbe abortProbe = nullptr;
  bool wasAborted = false;

  // Visible-offset counting, mirroring ChapterHtmlSlimParser (VisibleText.h).
  bool insideBody = false;
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

  // Snippets: a ring of recent original codepoints (before-context), and hits still
  // collecting after-context.
  size_t contextChars;
  std::vector<uint32_t> recent;  // ring, capacity contextChars + needle.size() + 1
  size_t recentCount = 0;
  struct Pending {
    size_t hitIndex;
    size_t remaining;
  };
  std::vector<Pending> pending;

  // Whole-word: a match whose last character is a word character waits here for the unit
  // after it, which decides whether the word ended there.
  Options options;
  bool endNeedsBoundary = false;
  bool hasCandidate = false;
  Hit candidate;

  size_t maxHits;
  std::vector<Hit> found;
};

}  // namespace booksearch
