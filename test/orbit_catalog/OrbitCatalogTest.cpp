#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "ChargeSession.h"
#include "OpdsOnDevice.h"
#include "OrbitCatalogFormat.h"

using namespace OrbitCatalog;

namespace {
Book sampleBook() {
  Book b;
  b.id = "urn:uuid:42";
  b.title = "Foundation\tand Empire";
  b.author = "Isaac Asimov";
  b.series = "Foundation";
  b.seriesIndex = "2";
  b.format = "epub";
  b.updated = "2026-11-02T10:00:00Z";
  b.url = "https://books.example/opds/b/42.epub";
  b.cover = "https://books.example/c/42.jpg";
  b.size = 98765;
  b.gen = 3;
  return b;
}
}  // namespace

TEST(OrbitCatalogRecord, RoundTrip) {
  std::string line;
  ASSERT_TRUE(appendRecord(sampleBook(), line));
  ASSERT_EQ(line.back(), '\n');
  EXPECT_EQ(std::count(line.begin(), line.end(), '\n'), 1);
  RecordView view;
  ASSERT_TRUE(splitRecord(std::string_view(line).substr(0, line.size() - 1), view));
  Book out;
  toBook(view, out);
  EXPECT_EQ(out.title, "Foundation and Empire");  // the tab is replaced
  EXPECT_EQ(out.author, "Isaac Asimov");
  EXPECT_EQ(out.series, "Foundation");
  EXPECT_EQ(out.seriesIndex, "2");
  EXPECT_EQ(out.url, "https://books.example/opds/b/42.epub");
  EXPECT_EQ(out.cover, "https://books.example/c/42.jpg");
  EXPECT_EQ(out.size, 98765u);
  EXPECT_EQ(out.gen, 3);
  EXPECT_EQ(view.gen(), 3);
}

TEST(OrbitCatalogRecord, RejectsUnfetchableAndDamaged) {
  Book b = sampleBook();
  b.url.clear();
  std::string line;
  EXPECT_FALSE(appendRecord(b, line));
  EXPECT_TRUE(line.empty());
  b = sampleBook();
  b.url.assign(kMaxUrl + 1, 'x');
  EXPECT_FALSE(appendRecord(b, line));
  RecordView view;
  EXPECT_FALSE(splitRecord("B\t1\tid\ttitle", view));
  EXPECT_FALSE(splitRecord("X\t1\tid\tt\ta\ts\ti\tf\t0\tu\turl\tc", view));
  EXPECT_TRUE(splitRecord("B\t1\tid\tt\ta\ts\ti\tf\t0\tu\turl\tc\r", view));
}

TEST(OrbitCatalogRecord, Header) {
  std::string line;
  appendHeader({123, 4, 1000, 2000}, kCatalogKind, line);
  Header h;
  ASSERT_TRUE(parseHeader(line.substr(0, line.size() - 1), kCatalogKind, h));
  EXPECT_EQ(h.count, 123u);
  EXPECT_EQ(h.gen, 4);
  EXPECT_EQ(h.fullTime, 1000u);
  EXPECT_EQ(h.updateTime, 2000u);
  EXPECT_FALSE(parseHeader(line, kWaitingKind, h));
  EXPECT_FALSE(parseHeader("ORBITCAT\t2\t1\t1\t0\t0", kCatalogKind, h));  // a future version
  EXPECT_FALSE(parseHeader("garbage", kCatalogKind, h));
}

// "Last updated": the later of the build and the update; 0 (shown as the book count) when neither
// stamp is a real time, which is what a sync with an untrusted clock wrote.
TEST(OrbitCatalogStamp, LastUpdatedTakesTheLaterRealTime) {
  EXPECT_EQ(lastUpdated({10, 1, 0, 0}), 0u);
  EXPECT_EQ(lastUpdated({10, 2, 1767225600, 0}), 1767225600u);           // update stamped without a clock
  EXPECT_EQ(lastUpdated({10, 2, 0, 1767225600}), 1767225600u);           // build stamped without a clock
  EXPECT_EQ(lastUpdated({10, 2, 1767225600, 1767312000}), 1767312000u);  // update after build
  EXPECT_EQ(lastUpdated({10, 2, 1767312000, 1767225600}), 1767312000u);  // a rebuild after the update
  EXPECT_EQ(lastUpdated({10, 1, 3600, 86400}), 0u);                      // 1970: an unset clock
}

TEST(OrbitCatalogStamp, FormatsLocalTime) {
  setenv("TZ", "UTC0", 1);
  tzset();
  char buf[24];
  ASSERT_TRUE(formatStamp(1794747900, buf, sizeof(buf)));  // 2026-11-15 13:05 UTC
  EXPECT_STREQ(buf, "2026-11-15 13:05");
  setenv("TZ", "EST5", 1);
  tzset();
  ASSERT_TRUE(formatStamp(1794747900, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "2026-11-15 08:05");
  setenv("TZ", "UTC0", 1);
  tzset();
  EXPECT_FALSE(formatStamp(0, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "");
  EXPECT_FALSE(formatStamp(1000, buf, sizeof(buf)));
  EXPECT_STREQ(buf, "");
  char small[8];
  ASSERT_TRUE(formatStamp(1794747900, small, sizeof(small)));  // cut, never overrun
  EXPECT_STREQ(small, "2026-11");
}

TEST(OrbitCatalogSeries, OrderKey) {
  EXPECT_EQ(seriesOrderKey("2"), 2000);
  EXPECT_EQ(seriesOrderKey("2.5"), 2500);
  EXPECT_EQ(seriesOrderKey("10.0"), 10000);
  EXPECT_EQ(seriesOrderKey(" 3"), 3000);
  EXPECT_EQ(seriesOrderKey(""), INT32_MAX);
  EXPECT_EQ(seriesOrderKey("x"), INT32_MAX);
  std::vector<std::string> idx{"10", "2", "", "1.5", "1"};
  std::sort(idx.begin(), idx.end(),
            [](const std::string& a, const std::string& b) { return seriesOrderKey(a) < seriesOrderKey(b); });
  EXPECT_EQ(idx, (std::vector<std::string>{"1", "1.5", "2", "10", ""}));
}

TEST(OrbitCatalogSeries, FromSummary) {
  std::string name, index;
  ASSERT_TRUE(seriesFromSummary("RATING: 4\nSERIES: The Expanse [3]\nTAGS: sf", name, index));
  EXPECT_EQ(name, "The Expanse");
  EXPECT_EQ(index, "3");
  ASSERT_TRUE(seriesFromSummary("Series: Discworld #12", name, index));
  EXPECT_EQ(name, "Discworld");
  EXPECT_EQ(index, "12");
  EXPECT_FALSE(seriesFromSummary("A tale of two cities.", name, index));
}

TEST(OrbitCatalogSearch, CaseAndAccentInsensitive) {
  std::string needle;
  fold("  Éowyn   OF ", needle);
  EXPECT_EQ(needle, "eowyn of");
  EXPECT_TRUE(containsFolded("The Lay of EOWYN of Rohan", needle));
  EXPECT_TRUE(containsFolded("Lady Éowyn of Rohan", needle));
  fold("garcia marquez", needle);
  EXPECT_TRUE(containsFolded("Gabriel García Márquez", needle));
  fold("", needle);
  EXPECT_TRUE(containsFolded("anything", needle));
  fold("zz", needle);
  EXPECT_FALSE(containsFolded("Gabriel García Márquez", needle));
}

TEST(OrbitCatalogSearch, SortKeyOrdersNames) {
  char a[kSortKeyLength], b[kSortKeyLength];
  sortKey("Émile Zola", a);
  sortKey("anne rice", b);
  EXPECT_GT(memcmp(a, b, kSortKeyLength), 0);
  EXPECT_EQ(std::string(a, 5), "emile");
}

TEST(OrbitCatalogRoot, MatchesRolesTolerantly) {
  EXPECT_TRUE(matchesRole("All Books", RootRole::AllBooks));
  EXPECT_TRUE(matchesRole("ALL BOOKS", RootRole::AllBooks));
  EXPECT_FALSE(matchesRole("Authors", RootRole::AllBooks));
  EXPECT_TRUE(matchesRole("Recent Books", RootRole::Recent));
  EXPECT_TRUE(matchesRole("Latest additions", RootRole::Recent));
  EXPECT_FALSE(matchesRole("Random Books", RootRole::Recent));
  EXPECT_TRUE(matchesRole("Collections", RootRole::Collections));
  EXPECT_FALSE(matchesRole("SmartScopes", RootRole::Collections));
  EXPECT_TRUE(isSendCollection("Send to X4"));
  EXPECT_TRUE(isSendCollection("send-to-x4 "));
  EXPECT_FALSE(isSendCollection("Send to Kindle"));
}

TEST(OrbitCatalogMerge, StopsAfterKnownRun) {
  std::vector<uint32_t> known{idHash("a"), idHash("b"), idHash("c"), idHash("d")};
  std::sort(known.begin(), known.end());
  MergeStop stop(known);
  EXPECT_EQ(stop.offer("new1"), MergeStop::Verdict::New);
  EXPECT_EQ(stop.offer("a"), MergeStop::Verdict::Known);     // an edited old book moved up
  EXPECT_EQ(stop.offer("new2"), MergeStop::Verdict::New);    // the run starts again
  EXPECT_EQ(stop.offer("new2"), MergeStop::Verdict::Known);  // repeated on the next page
  EXPECT_EQ(stop.offer("b"), MergeStop::Verdict::Known);
  EXPECT_EQ(stop.offer("c"), MergeStop::Verdict::Known);
  EXPECT_EQ(stop.offer("d"), MergeStop::Verdict::Stop);
  EXPECT_EQ(stop.newCount(), 2);
}

TEST(OrbitCatalogMerge, TooManyNew) {
  std::vector<uint32_t> known;
  MergeStop stop(known);
  for (int i = 0; i < MergeStop::kMaxNew; ++i) {
    ASSERT_EQ(stop.offer("id" + std::to_string(i)), MergeStop::Verdict::New);
  }
  EXPECT_EQ(stop.offer("one more"), MergeStop::Verdict::TooMany);
}

TEST(OrbitCatalogFormats, ExtensionAndMime) {
  EXPECT_STREQ(extensionFor("kepub"), ".kepub.epub");
  EXPECT_STREQ(extensionFor("epub"), ".epub");
  EXPECT_STREQ(extensionFor("xtch"), ".xtch");
  EXPECT_STREQ(mimeFor("epub"), "application/epub+zip");
  EXPECT_STREQ(mimeFor("txt"), "text/plain");
}

using namespace ChargeSession;

TEST(ChargeSessionPlan, RunsOncePerCharge) {
  State s;
  // Asleep on battery: timer wakes find no charger.
  EXPECT_EQ(plan(s, Wake::Timer, false, true, 100), Plan::Sleep);
  EXPECT_EQ(s.magic, State::kMagic);
  EXPECT_EQ(s.timerWakes, 1u);
  // Charger in: the next wake runs, once.
  EXPECT_EQ(plan(s, Wake::Timer, true, true, 200), Plan::Run);
  finished(s, true);
  EXPECT_EQ(plan(s, Wake::Timer, true, true, 300), Plan::Sleep);
  // Still charging 12 h later: visited again.
  EXPECT_EQ(plan(s, Wake::Timer, true, true, 200 + kRepeatSec), Plan::Run);
  finished(s, true);
  // Unplugged, then a new charge.
  EXPECT_EQ(plan(s, Wake::Timer, false, true, 200 + kRepeatSec + 10), Plan::Sleep);
  EXPECT_EQ(plan(s, Wake::Timer, true, true, 200 + kRepeatSec + 20), Plan::Run);
  EXPECT_EQ(s.sessions, 3u);
}

TEST(ChargeSessionPlan, NeverRunsOnBatteryOrDisarmed) {
  State s;
  EXPECT_EQ(plan(s, Wake::UsbPlug, false, true, 5), Plan::Sleep);
  EXPECT_EQ(plan(s, Wake::Timer, true, false, 5), Plan::Sleep);
  EXPECT_EQ(s.sessions, 0u);
}

TEST(ChargeSessionPlan, PlugWakeStartsNewCharge) {
  State s;
  reset(s);
  s.chargeDone = 1;
  s.lastRunSec = 10;
  EXPECT_EQ(plan(s, Wake::UsbPlug, true, true, 11), Plan::Run);
  EXPECT_EQ(s.plugWakes, 1u);
}

TEST(ChargeSessionPlan, FailuresRetryThenGiveUp) {
  State s;
  ASSERT_EQ(plan(s, Wake::Timer, true, true, 10), Plan::Run);
  finished(s, false);
  EXPECT_EQ(nextWakeSec(s, true, true, 11, 3 * 3600), kRetrySec);
  ASSERT_EQ(plan(s, Wake::Timer, true, true, 20), Plan::Run);
  finished(s, false);
  ASSERT_EQ(plan(s, Wake::Timer, true, true, 30), Plan::Run);
  finished(s, false);
  EXPECT_EQ(plan(s, Wake::Timer, true, true, 40), Plan::Sleep);
  EXPECT_EQ(nextWakeSec(s, true, true, 41, 3 * 3600), 3u * 3600);
}

TEST(ChargeSessionPlan, NextWake) {
  State s;
  EXPECT_EQ(nextWakeSec(s, true, false, 0, 3600), 0u);       // feature off: no timer
  EXPECT_EQ(nextWakeSec(s, true, true, 0, 3600), kSoonSec);  // asleep on the charger, due
  EXPECT_EQ(nextWakeSec(s, false, true, 0, 3600), 3600u);    // on battery: just look now and then
  ASSERT_EQ(plan(s, Wake::Timer, true, true, 1), Plan::Run);
  finished(s, true);
  EXPECT_EQ(nextWakeSec(s, true, true, 5, 3600), 3600u);
  // Unplugged while awake, then put to sleep on a charger again: due again.
  EXPECT_EQ(nextWakeSec(s, false, true, 6, 3600), 3600u);
  EXPECT_EQ(nextWakeSec(s, true, true, 7, 3600), kSoonSec);
}

TEST(ChargeSessionSummary, RoundTrip) {
  Summary s;
  s.arrived = 2;
  s.failed = 1;
  s.waiting = 1;
  s.newInCatalog = 5;
  s.problem = Problem::Server;
  s.when = 1763200000;
  s.addName("Leviathan Wakes");
  s.addName("Caliban's\tWar");
  s.addName("Abaddon's Gate");
  s.addName("dropped: only three are kept");
  std::string text;
  encodeSummary(s, text);
  Summary out;
  ASSERT_TRUE(decodeSummary(text, out));
  EXPECT_FALSE(out.shown);
  EXPECT_EQ(out.arrived, 2);
  EXPECT_EQ(out.failed, 1);
  EXPECT_EQ(out.waiting, 1);
  EXPECT_EQ(out.newInCatalog, 5);
  EXPECT_EQ(out.problem, Problem::Server);
  EXPECT_EQ(out.when, 1763200000u);
  ASSERT_EQ(out.nameCount, 3);
  EXPECT_STREQ(out.names[1], "Caliban's War");
  EXPECT_TRUE(out.worthShowing());
  EXPECT_FALSE(decodeSummary("nonsense", out));
}

TEST(ChargeSessionSummary, NameCutOnCharacterBoundary) {
  Summary s;
  std::string longTitle(Summary::kNameLength - 2, 'a');
  longTitle += "\xC3\xA9\xC3\xA9";  // two é straddling the cut
  s.addName(longTitle);
  const std::string name = s.names[0];
  EXPECT_LT(name.size(), Summary::kNameLength);
  EXPECT_NE(static_cast<unsigned char>(name.back()) & 0xC0, 0xC0);  // never a dangling lead byte
}

// --- On device: the name rules the card scan applies -------------------------------------------

TEST(OpdsOnDevice, NameHashIgnoresExtensionAndCase) {
  using OpdsOnDevice::nameHash;
  EXPECT_EQ(nameHash("Isaac Asimov - Foundation.epub"), nameHash("isaac asimov - FOUNDATION.xtc"));
  EXPECT_EQ(nameHash("Isaac Asimov - Foundation.epub"), nameHash("Isaac Asimov - Foundation.x"));
  EXPECT_NE(nameHash("Isaac Asimov - Foundation.epub"), nameHash("Isaac Asimov - Foundation 2.epub"));
  EXPECT_EQ(nameHash(".hidden"), nameHash(".hidden"));  // a leading dot is not an extension
}

TEST(OpdsOnDevice, BookFileNames) {
  EXPECT_TRUE(OpdsOnDevice::isBookFileName("a.EPUB"));
  EXPECT_TRUE(OpdsOnDevice::isBookFileName("a.kepub"));
  EXPECT_TRUE(OpdsOnDevice::isBookFileName("a.xtch"));
  EXPECT_FALSE(OpdsOnDevice::isBookFileName("a.jpg"));
  EXPECT_FALSE(OpdsOnDevice::isBookFileName("README"));
}

TEST(OpdsOnDevice, SubfoldersScanned) {
  EXPECT_TRUE(OpdsOnDevice::scansSubfolder("Sci-Fi", false));
  EXPECT_TRUE(OpdsOnDevice::scansSubfolder("Sci-Fi", true));
  EXPECT_FALSE(OpdsOnDevice::scansSubfolder(".crosspoint", true));
  EXPECT_FALSE(OpdsOnDevice::scansSubfolder("", false));
  // /COMPLETED is scanned on its own (its books are the read ones), not twice from the root.
  EXPECT_FALSE(OpdsOnDevice::scansSubfolder("COMPLETED", true));
  EXPECT_FALSE(OpdsOnDevice::scansSubfolder("completed", true));
  EXPECT_TRUE(OpdsOnDevice::scansSubfolder("COMPLETED", false));  // /Books/COMPLETED: just a folder
}

TEST(OpdsOnDevice, AddKeepsTheSetSortedAndUnique) {
  std::vector<uint32_t> set;
  OpdsOnDevice::add(set, "/Books/B - Two.epub");
  OpdsOnDevice::add(set, "/Books/A - One.epub");
  OpdsOnDevice::add(set, "A - One.xtc");  // the same book in another format
  ASSERT_EQ(set.size(), 2u);
  EXPECT_TRUE(std::is_sorted(set.begin(), set.end()));
  EXPECT_TRUE(OpdsOnDevice::containsHash(set, OpdsOnDevice::nameHash("a - one.epub")));
  EXPECT_FALSE(OpdsOnDevice::containsHash(set, OpdsOnDevice::nameHash("C - Three.epub")));
  EXPECT_FALSE(OpdsOnDevice::containsHash({}, OpdsOnDevice::nameHash("A - One.epub")));
}
