#include <Epub/BookTextSearch.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using booksearch::ChapterSearcher;
using booksearch::foldQuery;
using booksearch::Hit;

namespace {

std::vector<Hit> search(const std::string& xhtml, const std::string& query, size_t maxHits = 50,
                        bool byteAtATime = false) {
  ChapterSearcher searcher(foldQuery(query), maxHits);
  EXPECT_TRUE(searcher.ok());
  if (byteAtATime) {
    for (const char c : xhtml) searcher.write(static_cast<uint8_t>(c));
  } else {
    searcher.write(reinterpret_cast<const uint8_t*>(xhtml.data()), xhtml.size());
  }
  searcher.finish();
  return searcher.hits();
}

std::string doc(const std::string& body) { return "<html><head><title>T</title></head><body>" + body + "</body></html>"; }

std::vector<uint32_t> cps(const char32_t* s) {
  std::vector<uint32_t> out;
  for (; *s; ++s) out.push_back(static_cast<uint32_t>(*s));
  return out;
}

}  // namespace

TEST(BookTextSearchTest, QueryFoldsCaseAccentsAndWhitespace) {
  EXPECT_EQ(foldQuery("  Café   CRÈME\tbrûlée "), cps(U"cafe creme brulee"));
  EXPECT_EQ(foldQuery("Łódź Žižkov"), cps(U"lodz zizkov"));
  EXPECT_EQ(foldQuery("it’s “fine” — ok"), cps(U"it's \"fine\" - ok"));
  EXPECT_EQ(foldQuery("ПРИВЕТ Ωμέγα"), cps(U"привет ωμέγα"));
  EXPECT_EQ(foldQuery("co­op"), cps(U"coop"));  // soft hyphen ignored
  EXPECT_TRUE(foldQuery("   ­ ").empty());
}

TEST(BookTextSearchTest, FindsMatchAtVisibleOffset) {
  const auto hits = search(doc("<p>Hello world</p>"), "WORLD");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 6u);
  EXPECT_EQ(hits[0].visibleEnd, 11u);
}

TEST(BookTextSearchTest, HeadTextIsNotCounted) {
  // <title> sits outside <body>: offsets start at the body's first character.
  const auto hits = search(doc("<p>abc</p>"), "abc");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 0u);
}

TEST(BookTextSearchTest, MarkupWhitespaceCountsTowardOffsets) {
  // Like the layout parser, every body codepoint counts, including the
  // indentation between tags.
  const auto hits = search("<html><body>\n  <p>Hi there</p>\n</body></html>", "there");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 6u);  // "\n  " + "Hi "
}

TEST(BookTextSearchTest, MatchesAcrossInlineMarkup) {
  const auto hits = search(doc("<p>Hel<i>lo</i> wor<b>ld</b></p>"), "hello world");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 0u);
  EXPECT_EQ(hits[0].visibleEnd, 11u);
}

TEST(BookTextSearchTest, BlockBoundariesSeparateWords) {
  EXPECT_TRUE(search(doc("<p>end</p><p>start</p>"), "endstart").empty());
  const auto hits = search(doc("<p>end</p><p>start</p>"), "end start");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 0u);
  EXPECT_EQ(hits[0].visibleEnd, 8u);  // "end" + "start": the boundary space has no codepoint
  EXPECT_EQ(search(doc("<p>line one<br/>line two</p>"), "one line").size(), 1u);
}

TEST(BookTextSearchTest, IgnoresScriptAndStyleText) {
  const auto hits = search(doc("<style>.needle{}</style><script>var needle;</script><p>needle</p>"), "needle");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 0u);
}

TEST(BookTextSearchTest, ExpandsHtmlEntitiesLikeTheLayoutParser) {
  // EPUB chapters declare an external XHTML DTD; expat then hands HTML
  // entities it cannot resolve to the default handler instead of failing.
  const std::string xhtml =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body><p>caf&eacute;&nbsp;&amp; more</p></body></html>";
  const auto hits = search(xhtml, "cafe & more");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_EQ(hits[0].visibleOffset, 0u);
  EXPECT_EQ(hits[0].visibleEnd, 11u);  // é, nbsp and & each count as one codepoint
}

TEST(BookTextSearchTest, AccentsAndCaseInTheBookMatchAPlainQuery) {
  EXPECT_EQ(search(doc("<p>The CAFÉ was open.</p>"), "cafe").size(), 1u);
  EXPECT_EQ(search(doc("<p>He said “hello”.</p>"), "\"hello\"").size(), 1u);
}

TEST(BookTextSearchTest, CollapsesWhitespaceRunsInTheBook) {
  EXPECT_EQ(search(doc("<p>two\n\n   words</p>"), "two words").size(), 1u);
}

TEST(BookTextSearchTest, NonOverlappingAndCapped) {
  EXPECT_EQ(search(doc("<p>aaaa</p>"), "aa").size(), 2u);
  const std::string many = doc("<p>cat cat cat cat cat cat</p>");
  EXPECT_EQ(search(many, "cat", 3).size(), 3u);
}

TEST(BookTextSearchTest, SnippetShowsContextAroundTheMatch) {
  const auto hits = search(doc("<p>It was the best of times, it was the worst of times.</p>"), "best");
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_NE(hits[0].snippet.find("the best of"), std::string::npos);
  EXPECT_EQ(hits[0].snippet.rfind("It was", 0), 0u);  // whole lead-in fits: no leading ellipsis
}

TEST(BookTextSearchTest, ByteAtATimeMatchesWholeBuffer) {
  const std::string text = doc("<p>Ünïcödé <em>spans</em> and caf&eacute;</p>");
  const auto whole = search(text, "unicode spans");
  const auto bytes = search(text, "unicode spans", 50, true);
  ASSERT_EQ(whole.size(), 1u);
  ASSERT_EQ(bytes.size(), 1u);
  EXPECT_EQ(whole[0].visibleOffset, bytes[0].visibleOffset);
  EXPECT_EQ(whole[0].visibleEnd, bytes[0].visibleEnd);
}
