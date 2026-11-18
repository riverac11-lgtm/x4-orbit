#include <gtest/gtest.h>

#include <cstring>

#include "util/ReadingInsights.h"

using namespace ReadingInsights;

namespace {

const char* kBookA = "0123456789abcdef0123456789abcdef";
const char* kBookB = "fedcba9876543210fedcba9876543210";

BookReadingStats book(const char* id, const char* title, const char* author, const uint32_t seconds) {
  BookReadingStats b;
  b.docId = id;
  b.title = title;
  b.author = author;
  b.totalSeconds = seconds;
  b.pagesTurned = seconds / 60;
  b.sessions = 1;
  return b;
}

}  // namespace

// ---- The recent sessions ---------------------------------------------------------------------

TEST(SessionLog, ShortSessionsAreNotLogged) {
  SessionLog log;
  EXPECT_FALSE(add(log, kBookA, kMinSeconds - 1, 3));
  EXPECT_EQ(log.count, 0);
  EXPECT_EQ(newest(log), nullptr);
  EXPECT_STREQ(log.lastDocId, "");
  EXPECT_TRUE(add(log, kBookA, kMinSeconds, 3));
  EXPECT_EQ(log.count, 1);
  EXPECT_STREQ(log.lastDocId, kBookA);
}

TEST(SessionLog, KeepsTheNewestFourteenOldestFirst) {
  SessionLog log;
  for (uint32_t i = 1; i <= kCapacity + 3; ++i) ASSERT_TRUE(add(log, i % 2 ? kBookA : kBookB, i * 60, i));
  ASSERT_EQ(log.count, kCapacity);
  EXPECT_EQ(log.sessions[0].seconds, 4u * 60);             // the three oldest went
  EXPECT_EQ(newest(log)->seconds, (kCapacity + 3u) * 60);  // newest last
  EXPECT_EQ(newest(log)->pages, kCapacity + 3u);
  EXPECT_STREQ(log.lastDocId, (kCapacity + 3) % 2 ? kBookA : kBookB);
  EXPECT_EQ(longestSeconds(log), (kCapacity + 3u) * 60);
  // 4..17 minutes: the average is 10.5 minutes.
  EXPECT_EQ(averageSeconds(log), 630u);
}

TEST(SessionLog, AverageAndLongestOfNothingAreZero) {
  SessionLog log;
  EXPECT_EQ(averageSeconds(log), 0u);
  EXPECT_EQ(longestSeconds(log), 0u);
}

TEST(SessionLog, ASealedLogIsValidAndADamagedOneIsNot) {
  SessionLog log;
  add(log, kBookA, 600, 10);
  add(log, kBookB, 1200, 20);
  seal(log);
  EXPECT_TRUE(valid(log));

  SessionLog torn = log;
  torn.sessions[1].seconds = 5;  // a byte changed on the card
  EXPECT_FALSE(valid(torn));

  SessionLog foreign = log;
  foreign.magic = 0x12345678;
  EXPECT_FALSE(valid(foreign));

  SessionLog overfull = log;
  overfull.count = kCapacity + 1;
  seal(overfull);
  EXPECT_FALSE(valid(overfull));

  SessionLog zeros;
  std::memset(&zeros, 0, sizeof(zeros));
  EXPECT_FALSE(valid(zeros));
}

TEST(SessionLog, ALongDocIdIsCutNotOverrun) {
  SessionLog log;
  const std::string longId(80, 'x');
  ASSERT_TRUE(add(log, longId.c_str(), 600, 1));
  EXPECT_EQ(std::strlen(log.lastDocId), sizeof(log.lastDocId) - 1);
  seal(log);
  EXPECT_TRUE(valid(log));
}

// ---- Time left ---------------------------------------------------------------------------------

TEST(TimeLeft, HiddenUntilThereIsEnoughToGoOn) {
  EXPECT_FALSE(timeLeft(kEstimateMinSeconds - 1, 30, 1800).known);    // under ten minutes read
  EXPECT_FALSE(timeLeft(3600, kEstimateMinPercent - 1, 1800).known);  // under 2%
  EXPECT_FALSE(timeLeft(3600, 100, 1800).known);                      // finished
  EXPECT_TRUE(timeLeft(kEstimateMinSeconds, kEstimateMinPercent, 1800).known);
}

TEST(TimeLeft, FollowsTheBooksOwnPace) {
  // An hour for 20%: four hours for the 80% left, eight 30-minute sessions.
  const TimeLeft left = timeLeft(3600, 20, 1800);
  EXPECT_TRUE(left.known);
  EXPECT_EQ(left.seconds, 4u * 3600);
  EXPECT_EQ(left.sessions, 8);
  // A part session counts as one more.
  EXPECT_EQ(timeLeft(3600, 20, 1700).sessions, 9);
  // Nearly done: one session, never zero.
  EXPECT_EQ(timeLeft(9900, 99, 1800).sessions, 1);
  // No sessions logged yet: the time without the count.
  const TimeLeft noPace = timeLeft(3600, 20, 0);
  EXPECT_TRUE(noPace.known);
  EXPECT_EQ(noPace.sessions, 0);
}

// ---- Milestones -----------------------------------------------------------------------------

TEST(Milestones, ThresholdsRunInOnesTwoAndAHalvesAndFives) {
  const uint32_t hours[] = {10, 25, 50, 100, 250, 500, 1000, 2500};
  uint32_t v = 0;
  for (const uint32_t t : hours) {
    v = nextThreshold(Kind::Hours, v);
    EXPECT_EQ(v, t);
  }
  const uint32_t pages[] = {1000, 2500, 5000, 10000, 25000, 50000, 100000};
  v = 0;
  for (const uint32_t t : pages) {
    v = nextThreshold(Kind::Pages, v);
    EXPECT_EQ(v, t);
  }
  const uint32_t books[] = {5, 10, 15, 25, 50, 100, 250, 500};
  v = 0;
  for (const uint32_t t : books) {
    v = nextThreshold(Kind::Books, v);
    EXPECT_EQ(v, t);
  }
  EXPECT_EQ(nextThreshold(Kind::Pages, 999), 1000u);
  EXPECT_EQ(nextThreshold(Kind::Books, 15), 25u);
  EXPECT_GT(nextThreshold(Kind::Pages, 4000000000u), 4000000000u);  // no wrap at the top
}

TEST(Milestones, HoursCountInSeconds) {
  // 37 h 20 min read: next 50 h, 12 h 40 min to go, a little under half way from 25 to 50.
  const Milestone m = milestone(Kind::Hours, 37 * 3600 + 20 * 60);
  EXPECT_EQ(m.value, 37u);
  EXPECT_EQ(m.previous, 25u);
  EXPECT_EQ(m.next, 50u);
  EXPECT_EQ(m.toGo, 12u * 3600 + 40 * 60);
  EXPECT_EQ(m.permille, 493);
}

TEST(Milestones, AThresholdReachedPointsAtTheNext) {
  const Milestone pages = milestone(Kind::Pages, 1000);
  EXPECT_EQ(pages.previous, 1000u);
  EXPECT_EQ(pages.next, 2500u);
  EXPECT_EQ(pages.toGo, 1500u);
  EXPECT_EQ(pages.permille, 0);

  const Milestone none = milestone(Kind::Books, 0);
  EXPECT_EQ(none.previous, 0u);
  EXPECT_EQ(none.next, 5u);
  EXPECT_EQ(none.toGo, 5u);
  EXPECT_EQ(none.permille, 0);

  const Milestone books = milestone(Kind::Books, 12);
  EXPECT_EQ(books.next, 15u);
  EXPECT_EQ(books.toGo, 3u);
  EXPECT_EQ(books.permille, 400);
}

TEST(Milestones, ClosestIsTheFurthestAlong) {
  const Milestone ms[] = {milestone(Kind::Hours, 11 * 3600), milestone(Kind::Pages, 4800), milestone(Kind::Books, 1)};
  EXPECT_EQ(closest(ms, 3), 1u);  // 4,800 of 5,000 pages
  const Milestone even[] = {milestone(Kind::Hours, 0), milestone(Kind::Pages, 0), milestone(Kind::Books, 0)};
  EXPECT_EQ(closest(even, 3), 0u);  // ties: the first
}

// ---- Top books --------------------------------------------------------------------------------

TEST(TopBooks, DuplicatesByTitleAndAuthorAreMergedAndReordered) {
  std::vector<BookReadingStats> books = {
      book(kBookA, "Dune", "Frank Herbert", 5000),
      book("c1", "The Talisman", "Stephen King", 4000),
      book("c2", "Middlemarch", "George Eliot", 3500),
      book("c3", "the talisman", "STEPHEN KING", 3000),  // the same book under another file name
      book("c4", "The Talisman", "Peter Straub", 100),   // another author: another book
  };
  mergeDuplicates(books);
  ASSERT_EQ(books.size(), 4u);
  EXPECT_EQ(books[0].title, "The Talisman");
  EXPECT_EQ(books[0].totalSeconds, 7000u);
  EXPECT_EQ(books[0].sessions, 2u);
  EXPECT_EQ(books[0].pagesTurned, 4000u / 60 + 3000u / 60);
  EXPECT_EQ(books[1].title, "Dune");
  EXPECT_EQ(books[2].title, "Middlemarch");
  EXPECT_EQ(books[3].author, "Peter Straub");
}

TEST(TopBooks, UntitledBooksAreNeverMerged) {
  std::vector<BookReadingStats> books = {book(kBookA, "", "", 600), book(kBookB, "", "", 300)};
  mergeDuplicates(books);
  EXPECT_EQ(books.size(), 2u);
}

// ---- Formatting --------------------------------------------------------------------------------

TEST(Formatting, DurationsReadInHoursAndMinutes) {
  char out[24];
  formatDuration(out, sizeof(out), 42 * 60 + 59, "%d h %d min", "%d h", "%d min");
  EXPECT_STREQ(out, "42 min");
  formatDuration(out, sizeof(out), 2 * 3600 + 30, "%d h %d min", "%d h", "%d min");
  EXPECT_STREQ(out, "2 h");
  formatDuration(out, sizeof(out), 3600 + 52 * 60, "%d h %d min", "%d h", "%d min");
  EXPECT_STREQ(out, "1 h 52 min");
  formatDuration(out, sizeof(out), 20, "%d h %d min", "%d h", "%d min");
  EXPECT_STREQ(out, "0 min");
}

TEST(Formatting, ThousandsAreGrouped) {
  char out[16];
  groupThousands(out, sizeof(out), 0);
  EXPECT_STREQ(out, "0");
  groupThousands(out, sizeof(out), 999);
  EXPECT_STREQ(out, "999");
  groupThousands(out, sizeof(out), 1000);
  EXPECT_STREQ(out, "1,000");
  groupThousands(out, sizeof(out), 1234567);
  EXPECT_STREQ(out, "1,234,567");
  groupThousands(out, sizeof(out), 4294967295u);
  EXPECT_STREQ(out, "4,294,967,295");
}
