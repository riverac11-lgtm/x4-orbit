#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "OpdsDownloadQueue.h"
#include "OpdsSavedPosition.h"

// ---- Saved position ----

TEST(OpdsSavedPosition, RoundTrip) {
  const std::vector<std::string> history{"", "/opds/authors", "https://h/opds/authors?page=2"};
  std::string text;
  ASSERT_TRUE(OpdsPosition::encode("/opds/author/7", history, 12, text));
  OpdsSavedPosition pos;
  ASSERT_TRUE(OpdsPosition::decode(text, pos));
  EXPECT_EQ(pos.feed, "/opds/author/7");
  EXPECT_EQ(pos.history, history);
  EXPECT_EQ(pos.row, 12);
}

TEST(OpdsSavedPosition, RootWithNoWayBackIsNothingToRemember) {
  std::string text = "stale";
  EXPECT_FALSE(OpdsPosition::encode("", {}, 3, text));
  EXPECT_TRUE(text.empty());
}

TEST(OpdsSavedPosition, RowIsClamped) {
  std::string text;
  ASSERT_TRUE(OpdsPosition::encode("/a", {""}, -4, text));
  OpdsSavedPosition pos;
  ASSERT_TRUE(OpdsPosition::decode(text, pos));
  EXPECT_EQ(pos.row, 0);
  ASSERT_TRUE(OpdsPosition::encode("/a", {""}, 1000000, text));
  ASSERT_TRUE(OpdsPosition::decode(text, pos));
  EXPECT_EQ(pos.row, UINT16_MAX);
}

TEST(OpdsSavedPosition, DeepHistoryKeepsRootAndMostRecent) {
  std::vector<std::string> history{""};
  for (int i = 1; i < 30; ++i) history.push_back("/p" + std::to_string(i));
  std::string text;
  ASSERT_TRUE(OpdsPosition::encode("/last", history, 0, text));
  OpdsSavedPosition pos;
  ASSERT_TRUE(OpdsPosition::decode(text, pos));
  ASSERT_EQ(pos.history.size(), OpdsPosition::kMaxDepth);
  EXPECT_EQ(pos.history.front(), "");
  EXPECT_EQ(pos.history[1], "/p" + std::to_string(30 - (OpdsPosition::kMaxDepth - 1)));
  EXPECT_EQ(pos.history.back(), "/p29");
}

TEST(OpdsSavedPosition, UnstorableUrlIsRefused) {
  std::string text;
  EXPECT_FALSE(OpdsPosition::encode("/a\nb", {""}, 0, text));
  EXPECT_FALSE(OpdsPosition::encode("/a", {"", std::string(OpdsPosition::kMaxUrlLength + 1, 'x')}, 0, text));
  EXPECT_TRUE(OpdsPosition::encode("/a", {"", std::string(OpdsPosition::kMaxUrlLength, 'x')}, 0, text));
}

TEST(OpdsSavedPosition, DamagedFilesDecodeToNothing) {
  std::string text;
  ASSERT_TRUE(OpdsPosition::encode("/feed", {"", "/x"}, 5, text));
  OpdsSavedPosition pos;
  // Cut short anywhere: never a partial result.
  for (size_t cut = 0; cut + 1 < text.size(); ++cut) {
    const std::string part = text.substr(0, cut);
    if (!OpdsPosition::decode(part, pos)) {
      EXPECT_TRUE(pos.feed.empty());
      EXPECT_TRUE(pos.history.empty());
      continue;
    }
    // A cut right after a whole line can still be a valid shorter position; it must be a prefix.
    EXPECT_EQ(pos.feed, "/feed");
  }
  EXPECT_FALSE(OpdsPosition::decode("OPDSPOS2\n1\n/feed\n", pos));
  EXPECT_FALSE(OpdsPosition::decode("OPDSPOS1\nx\n/feed\n", pos));
  EXPECT_FALSE(OpdsPosition::decode("OPDSPOS1\n99999\n/feed\n", pos));
  EXPECT_FALSE(OpdsPosition::decode("OPDSPOS1\n1\n\n", pos));  // root, no way back
  EXPECT_FALSE(OpdsPosition::decode("OPDSPOS1\n1\n/feed\n/tail-without-newline", pos));
  EXPECT_FALSE(OpdsPosition::decode(std::string(OpdsPosition::kMaxFileBytes + 1, 'a'), pos));
}

TEST(OpdsSavedPosition, TooManyHistoryLinesAreRefused) {
  std::string text = "OPDSPOS1\n0\n/feed\n";
  for (size_t i = 0; i <= OpdsPosition::kMaxDepth; ++i) text += "/h\n";
  OpdsSavedPosition pos;
  EXPECT_FALSE(OpdsPosition::decode(text, pos));
}

// ---- Download queue records ----

TEST(OpdsQueue, ItemRoundTrip) {
  const OpdsQueueItem item{
      "Title", "Author", "https://h/get/1.epub", "epub", "application/epub+zip", "epub", "https://h/cover/1.jpg"};
  std::string record;
  ASSERT_TRUE(OpdsQueue::encodeItem(item, record));
  OpdsQueueItem back;
  ASSERT_TRUE(OpdsQueue::decodeItem(record, back));
  EXPECT_EQ(back.title, item.title);
  EXPECT_EQ(back.author, item.author);
  EXPECT_EQ(back.url, item.url);
  EXPECT_EQ(back.extension, item.extension);
  EXPECT_EQ(back.mimeType, item.mimeType);
  EXPECT_EQ(back.formatKey, item.formatKey);
  EXPECT_EQ(back.coverUrl, item.coverUrl);
}

TEST(OpdsQueue, LongTitleIsCutLongUrlRefused) {
  OpdsQueueItem item;
  item.title = std::string(1000, 't');
  item.url = "https://h/1";
  std::string record;
  ASSERT_TRUE(OpdsQueue::encodeItem(item, record));
  OpdsQueueItem back;
  ASSERT_TRUE(OpdsQueue::decodeItem(record, back));
  EXPECT_EQ(back.title.size(), 255u);

  item.url = std::string(OpdsQueue::kMaxFieldLength + 1, 'u');
  EXPECT_FALSE(OpdsQueue::encodeItem(item, record));
  item.url.clear();
  EXPECT_FALSE(OpdsQueue::encodeItem(item, record));
}

TEST(OpdsQueue, DamagedRecordsDecodeToNothing) {
  const OpdsQueueItem item{"T", "A", "https://h/1", "epub", "m", "epub", ""};
  std::string record;
  ASSERT_TRUE(OpdsQueue::encodeItem(item, record));
  OpdsQueueItem back;
  for (size_t cut = 0; cut < record.size(); ++cut) {
    EXPECT_FALSE(OpdsQueue::decodeItem(record.substr(0, cut), back)) << cut;
    EXPECT_TRUE(back.url.empty());
  }
  EXPECT_FALSE(OpdsQueue::decodeItem(record + "x", back));
}

TEST(OpdsQueue, KeyPrefersIdOverHref) {
  EXPECT_EQ(OpdsQueue::markKey("urn:1", "/a"), OpdsQueue::markKey("urn:1", "/b"));
  EXPECT_NE(OpdsQueue::markKey("urn:1", "/a"), OpdsQueue::markKey("urn:2", "/a"));
  EXPECT_EQ(OpdsQueue::markKey("", "/a"), OpdsQueue::markKey("", "/a"));
  EXPECT_NE(OpdsQueue::markKey("", "/a"), OpdsQueue::markKey("", "/b"));
}

// ---- Marks ----

TEST(OpdsMarks, AddRemoveKeepsOrder) {
  OpdsMarks marks;
  EXPECT_TRUE(marks.empty());
  EXPECT_TRUE(marks.add(10));
  EXPECT_TRUE(marks.add(20));
  EXPECT_TRUE(marks.add(30));
  EXPECT_FALSE(marks.add(20));  // already there
  EXPECT_EQ(marks.size(), 3u);
  EXPECT_TRUE(marks.remove(20));
  EXPECT_FALSE(marks.remove(20));
  ASSERT_EQ(marks.size(), 2u);
  EXPECT_EQ(marks.at(0), 10u);
  EXPECT_EQ(marks.at(1), 30u);
  EXPECT_TRUE(marks.contains(30));
  EXPECT_FALSE(marks.contains(20));
  marks.clear();
  EXPECT_TRUE(marks.empty());
}

TEST(OpdsMarks, Bounded) {
  OpdsMarks marks;
  for (uint32_t i = 0; i < OpdsQueue::kMaxMarks; ++i) EXPECT_TRUE(marks.add(i + 1));
  EXPECT_TRUE(marks.full());
  EXPECT_FALSE(marks.add(999));
  EXPECT_EQ(marks.size(), OpdsQueue::kMaxMarks);
  EXPECT_TRUE(marks.remove(1));
  EXPECT_TRUE(marks.add(999));
  EXPECT_EQ(marks.at(OpdsQueue::kMaxMarks - 1), 999u);
}
