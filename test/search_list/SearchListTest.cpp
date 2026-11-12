// The kept search result list (SearchList.h): the file round trip the search screen and the
// reader's Next match / Previous match rely on, the stepping rule, and the recent queries.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "activities/reader/SearchList.h"

namespace {

// The slice of HalFile the list uses, over a string.
struct MemFile {
  std::string data;
  size_t pos = 0;
  int read(void* out, const size_t count) {
    const size_t n = std::min(count, data.size() - pos);
    memcpy(out, data.data() + pos, n);
    pos += n;
    return static_cast<int>(n);
  }
  size_t write(const void* in, const size_t count) {
    if (data.size() < pos + count) data.resize(pos + count);
    memcpy(data.data() + pos, in, count);
    pos += count;
    return count;
  }
  bool seek(const size_t to) {
    if (to > data.size()) return false;
    pos = to;
    return true;
  }
};

MemFile sample() {
  MemFile file;
  searchlist::Header header;
  header.current = 1;
  header.count = 3;
  header.flags = searchlist::FLAG_WHOLE_WORD;
  header.skipped = 2;
  header.query = "caf\xC3\xA9 noir";
  EXPECT_TRUE(searchlist::writeHeader(file, header));
  EXPECT_TRUE(searchlist::writePos(file, {4, 100, 108}));
  EXPECT_TRUE(searchlist::writePos(file, {4, 70000, 70008}));
  EXPECT_TRUE(searchlist::writePos(file, {9, 5, 13}));
  EXPECT_TRUE(searchlist::writeText(file, "Chapter One", "\xE2\x80\xA6 a caf\xC3\xA9 noir, please"));
  EXPECT_TRUE(searchlist::writeText(file, "", "second"));
  EXPECT_TRUE(searchlist::writeText(file, "Chapter Two", ""));
  return file;
}

}  // namespace

TEST(SearchListTest, RoundTrip) {
  MemFile file = sample();
  file.pos = 0;
  searchlist::Header header;
  std::vector<searchlist::Pos> hits;
  ASSERT_TRUE(searchlist::readPositions(file, header, hits));
  EXPECT_EQ(header.current, 1);
  EXPECT_EQ(header.flags, searchlist::FLAG_WHOLE_WORD);
  EXPECT_EQ(header.skipped, 2);
  EXPECT_EQ(header.query, "caf\xC3\xA9 noir");
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[1].spine, 4);
  EXPECT_EQ(hits[1].offset, 70000u);
  EXPECT_EQ(hits[1].end, 70008u);
  EXPECT_EQ(hits[2].spine, 9);
  std::string title;
  std::string snippet;
  ASSERT_TRUE(searchlist::readText(file, title, snippet));
  EXPECT_EQ(title, "Chapter One");
  EXPECT_EQ(snippet, "\xE2\x80\xA6 a caf\xC3\xA9 noir, please");
  ASSERT_TRUE(searchlist::readText(file, title, snippet));
  EXPECT_EQ(title, "");
  EXPECT_EQ(snippet, "second");
  ASSERT_TRUE(searchlist::readText(file, title, snippet));
  EXPECT_EQ(title, "Chapter Two");
  EXPECT_FALSE(searchlist::readText(file, title, snippet));  // end of file
}

TEST(SearchListTest, CurrentIsRewrittenInPlace) {
  MemFile file = sample();
  const std::string before = file.data;
  ASSERT_TRUE(searchlist::writeCurrent(file, 2));
  EXPECT_EQ(file.data.size(), before.size());
  file.pos = 0;
  searchlist::Header header;
  std::vector<searchlist::Pos> hits;
  ASSERT_TRUE(searchlist::readPositions(file, header, hits));
  EXPECT_EQ(header.current, 2);
  EXPECT_EQ(header.query, "caf\xC3\xA9 noir");
  EXPECT_EQ(hits[0].offset, 100u);
}

TEST(SearchListTest, DamagedFilesAreRefused) {
  searchlist::Header header;
  std::vector<searchlist::Pos> hits;
  MemFile empty;
  EXPECT_FALSE(searchlist::readPositions(empty, header, hits));

  MemFile wrongMagic = sample();
  wrongMagic.data[0] ^= 1;
  wrongMagic.pos = 0;
  EXPECT_FALSE(searchlist::readPositions(wrongMagic, header, hits));

  // Cut inside the position table.
  MemFile cut = sample();
  cut.data.resize(40);
  cut.pos = 0;
  EXPECT_FALSE(searchlist::readPositions(cut, header, hits));

  // A count above the cap, or a current hit outside the list, is not ours.
  MemFile big = sample();
  big.data[6] = 81;
  big.pos = 0;
  EXPECT_FALSE(searchlist::readHeader(big, header));
  MemFile outside = sample();
  outside.data[4] = 3;
  outside.pos = 0;
  EXPECT_FALSE(searchlist::readHeader(outside, header));

  // A length above the cap is not ours either.
  MemFile longText;
  const uint16_t tooLong = searchlist::MAX_TEXT + 1;
  longText.write(&tooLong, sizeof(tooLong));
  longText.data.append(tooLong + 2u, 'x');
  longText.pos = 0;
  std::string title;
  std::string snippet;
  EXPECT_FALSE(searchlist::readText(longText, title, snippet));
}

TEST(SearchListTest, OverLongTextIsCutAtACharacterBoundary) {
  // 511 ASCII bytes, then a two-byte character straddling the cap.
  MemFile file;
  ASSERT_TRUE(searchlist::writeText(file, std::string(searchlist::MAX_TEXT - 1, 'x') + "\xC3\xA9tail", "ok"));
  file.pos = 0;
  std::string title;
  std::string snippet;
  ASSERT_TRUE(searchlist::readText(file, title, snippet));
  EXPECT_EQ(title, std::string(searchlist::MAX_TEXT - 1, 'x'));
  EXPECT_EQ(snippet, "ok");
}

TEST(SearchListTest, StepGoesRound) {
  using searchlist::step;
  EXPECT_EQ(step(-1, 3, true), 0);
  EXPECT_EQ(step(-1, 3, false), 2);
  EXPECT_EQ(step(0, 3, true), 1);
  EXPECT_EQ(step(2, 3, true), 0);
  EXPECT_EQ(step(0, 3, false), 2);
  EXPECT_EQ(step(2, 3, false), 1);
  EXPECT_EQ(step(0, 1, true), 0);
  EXPECT_EQ(step(0, 1, false), 0);
  EXPECT_EQ(step(0, 0, true), -1);
}

TEST(SearchListTest, RecentQueries) {
  std::vector<std::string> recent;
  EXPECT_TRUE(searchlist::addRecent(recent, "one"));
  EXPECT_TRUE(searchlist::addRecent(recent, "two"));
  EXPECT_FALSE(searchlist::addRecent(recent, "two"));  // already first: nothing to write
  EXPECT_FALSE(searchlist::addRecent(recent, ""));
  EXPECT_FALSE(searchlist::addRecent(recent, "a\nb"));
  EXPECT_TRUE(searchlist::addRecent(recent, "one"));  // moves to the front, no duplicate
  EXPECT_EQ(recent, (std::vector<std::string>{"one", "two"}));
  for (int i = 0; i < 10; ++i) searchlist::addRecent(recent, "q" + std::to_string(i));
  ASSERT_EQ(recent.size(), searchlist::MAX_RECENT);
  EXPECT_EQ(recent.front(), "q9");
  EXPECT_EQ(recent.back(), "q2");

  EXPECT_EQ(searchlist::parseRecent(searchlist::joinRecent(recent)), recent);
  EXPECT_EQ(searchlist::parseRecent("a b\r\n\nc"), (std::vector<std::string>{"a b", "c"}));
  EXPECT_TRUE(searchlist::parseRecent("").empty());
  // A longer file than we write is cut at the cap.
  EXPECT_EQ(searchlist::parseRecent("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n").size(), searchlist::MAX_RECENT);
}
