// Tests for the pure half of the Speed check (src/util/SpeedCheckLog.h).
//
// The Speed check's figures are read by someone who cannot check them against a serial log, so
// what a figure is paired with has to be right: a page turn with the refresh that showed it and
// not with a status-bar tick, a chapter change with the first page of that chapter, a build that
// finished after the first page with the record already on screen. None of that needs hardware.

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "util/SpeedCheck.h"  // kReportBytes; the rest of it is declarations only
#include "util/SpeedCheckLog.h"

namespace {

using namespace SpeedCheck;

// One page turn: released at `event`, acted on at `dispatch`, handed to the panel at `refresh`,
// settled at `visible`.
void turn(Log& log, uint32_t event, uint32_t dispatch, uint32_t refresh, uint32_t visible, bool preDrawn = false) {
  log.noteInput(event, dispatch, preDrawn);
  log.refreshStart(refresh);
  log.pageVisible(visible);
}

TEST(SpeedCheckTurns, SplitsATurnIntoWaitDrawAndRefresh) {
  Log log;
  turn(log, 1000, 1012, 1100, 1620);
  ASSERT_EQ(log.turnCount(), 1);
  const Turn& t = log.turn(0);
  EXPECT_EQ(t.waitMs, 12);
  EXPECT_EQ(t.renderMs, 88);
  EXPECT_EQ(t.refreshMs, 520);
  EXPECT_EQ(t.preDrawn, 0);
  EXPECT_EQ(log.turnStats(TurnField::ToRefresh).median, 100);
  EXPECT_EQ(log.turnStats(TurnField::Total).median, 620);
}

TEST(SpeedCheckTurns, ATurnWithoutAButtonIsTimedFromItsDispatch) {
  Log log;
  turn(log, 0, 5000, 5090, 5600);  // auto page turn, tilt, touch
  EXPECT_EQ(log.turn(0).waitMs, 0);
  EXPECT_EQ(log.turn(0).renderMs, 90);
}

TEST(SpeedCheckTurns, ARedrawWithoutATurnIsNotATurn) {
  Log log;
  log.refreshStart(100);  // a clock-minute status-bar tick
  log.pageVisible(600);
  EXPECT_EQ(log.turnCount(), 0);
  turn(log, 1000, 1010, 1100, 1600);
  log.refreshStart(60000);  // and another tick after a real turn
  log.pageVisible(60500);
  EXPECT_EQ(log.turnCount(), 1);
}

TEST(SpeedCheckTurns, AnInputDuringTheWaveformWaitsForItsOwnRefresh) {
  Log log;
  log.noteInput(1000, 1010, false);
  log.refreshStart(1100);
  log.noteInput(1300, 1310, false);  // the next press, while the panel is still refreshing
  log.pageVisible(1600);
  ASSERT_EQ(log.turnCount(), 1);
  EXPECT_EQ(log.turn(0).waitMs, 10);
  EXPECT_EQ(log.turn(0).refreshMs, 500);
  log.refreshStart(1700);
  log.pageVisible(2200);
  ASSERT_EQ(log.turnCount(), 2);
  EXPECT_EQ(log.turn(1).waitMs, 10);
  EXPECT_EQ(log.turn(1).renderMs, 390);  // it waited for the first page's refresh
}

TEST(SpeedCheckTurns, ACancelledOrStaleInputIsDropped) {
  Log log;
  log.noteInput(1000, 1010, false);
  log.cancelInput();  // back on the first page of the book: nothing is drawn
  log.refreshStart(2000);
  log.pageVisible(2500);
  EXPECT_EQ(log.turnCount(), 0);

  log.noteInput(3000, 3010, false);  // its page was never drawn (an error screen instead)
  log.refreshStart(3010 + 60000);
  log.pageVisible(3010 + 60500);
  EXPECT_EQ(log.turnCount(), 0);
}

TEST(SpeedCheckTurns, AnEventStampFromLongAgoFallsBackToTheDispatch) {
  Log log;
  turn(log, 1000, 1000 + 61000, 62100, 62600);
  EXPECT_EQ(log.turn(0).waitMs, 0);
}

TEST(SpeedCheckTurns, TimesSaturateAtSixteenBits) {
  Log log;
  turn(log, 1000, 1010, 1100, 1100 + 70000);
  EXPECT_EQ(log.turn(0).refreshMs, 0xFFFF);
  EXPECT_EQ(log.turnStats(TurnField::Total).max, 0xFFFF);
}

TEST(SpeedCheckTurns, MinMedianMaxOverAnOddAndAnEvenCount) {
  Log log;
  EXPECT_EQ(log.turnStats(TurnField::Refresh).count, 0);
  const uint32_t refreshes[] = {500, 1900, 480, 520, 510};
  uint32_t now = 1000;
  for (const uint32_t r : refreshes) {
    turn(log, now, now, now, now + r);
    now += 5000;
  }
  Stats s = log.turnStats(TurnField::Refresh);
  EXPECT_EQ(s.count, 5);
  EXPECT_EQ(s.min, 480);
  EXPECT_EQ(s.median, 510);
  EXPECT_EQ(s.max, 1900);

  turn(log, now, now, now, now + 530);
  s = log.turnStats(TurnField::Refresh);
  EXPECT_EQ(s.count, 6);
  EXPECT_EQ(s.median, 510);  // the lower middle of 480 500 510 520 530 1900
}

TEST(SpeedCheckTurns, TheRingKeepsTheLastTurnsOldestFirst) {
  Log log;
  uint32_t now = 1000;
  for (int i = 0; i < kTurnCapacity + 5; i++) {
    turn(log, now, now, now, now + 100 + static_cast<uint32_t>(i), /*preDrawn=*/i % 2 == 0);
    now += 1000;
  }
  ASSERT_EQ(log.turnCount(), kTurnCapacity);
  EXPECT_EQ(log.turn(0).refreshMs, 105);  // turns 0..4 fell out
  EXPECT_EQ(log.turn(kTurnCapacity - 1).refreshMs, 100 + kTurnCapacity + 4);
  const Stats s = log.turnStats(TurnField::Refresh);
  EXPECT_EQ(s.min, 105);
  EXPECT_EQ(s.max, 100 + kTurnCapacity + 4);
  EXPECT_EQ(log.preDrawnTurns(), kTurnCapacity / 2);
}

TEST(SpeedCheckChapter, ACachedChapterIsTimedFromTheTurnThatCrossedIntoIt) {
  Log log;
  log.noteInput(10000, 10015, false);
  log.sectionEntry(10030, 7);
  log.sectionProbed(/*needBuild=*/false);
  log.refreshStart(10100);
  log.pageVisible(10650);
  const ChapterRecord& c = log.lastChapter();
  ASSERT_TRUE(c.valid);
  EXPECT_EQ(c.totalMs, 650u);
  EXPECT_EQ(c.section.source, Source::Cached);
  EXPECT_EQ(c.section.spine, 7);
  EXPECT_FALSE(c.section.buildDone);
  EXPECT_EQ(log.turnCount(), 0);  // the turn became the chapter change, not a page turn
}

TEST(SpeedCheckChapter, AJumpWithoutATurnIsTimedFromTheSectionEntry) {
  Log log;
  log.sectionEntry(20000, 3);
  log.sectionProbed(false);
  log.refreshStart(20050);
  log.pageVisible(20600);
  EXPECT_EQ(log.lastChapter().totalMs, 600u);
}

TEST(SpeedCheckChapter, ABlockingBuildIsBuiltThenShown) {
  Log log;
  log.noteInput(1000, 1010, false);
  log.sectionEntry(1020, 4);
  log.sectionProbed(/*needBuild=*/true);
  log.sectionBuilt(3320, 4, 14);
  log.refreshStart(3400);
  log.pageVisible(3950);
  const ChapterRecord& c = log.lastChapter();
  EXPECT_EQ(c.totalMs, 2950u);
  EXPECT_EQ(c.section.source, Source::Built);
  EXPECT_TRUE(c.section.buildDone);
  EXPECT_EQ(c.section.buildMs, 2300u);
  EXPECT_EQ(c.section.pages, 14);
}

TEST(SpeedCheckChapter, ALiveBuildShowsItsFirstPageAndIsCompletedLater) {
  Log log;
  log.sectionEntry(1000, 9);
  log.sectionProbed(true);
  log.refreshStart(1800);  // the first page out of the build in progress
  log.pageVisible(2400);
  EXPECT_EQ(log.lastChapter().totalMs, 1400u);
  EXPECT_EQ(log.lastChapter().section.source, Source::BuiltLive);
  EXPECT_FALSE(log.lastChapter().section.buildDone);

  turn(log, 3000, 3010, 3100, 3600);  // a page turn inside the chapter while it builds
  log.sectionBuilt(6200, 9, 31);
  EXPECT_TRUE(log.lastChapter().section.buildDone);
  EXPECT_EQ(log.lastChapter().section.buildMs, 5200u);
  EXPECT_EQ(log.lastChapter().section.pages, 31);
  EXPECT_EQ(log.lastChapter().section.source, Source::BuiltLive);
  EXPECT_EQ(log.lastChapter().totalMs, 1400u);  // the first page's time stands
  EXPECT_EQ(log.turnCount(), 1);
}

TEST(SpeedCheckChapter, ABuildOfAnotherChapterDoesNotCompleteTheRecord) {
  Log log;
  log.sectionEntry(1000, 9);
  log.sectionProbed(true);
  log.refreshStart(1800);
  log.pageVisible(2400);
  log.sectionBuilt(9000, 10, 5);
  EXPECT_FALSE(log.lastChapter().section.buildDone);
}

TEST(SpeedCheckChapter, ARetriedEntryOfTheSameChapterKeepsItsStart) {
  Log log;
  log.noteInput(1000, 1010, false);
  log.sectionEntry(1020, 4);
  log.sectionProbed(true);
  log.sectionEntry(2500, 4);  // the build fell back to another path and entered again
  log.sectionProbed(true);
  log.sectionBuilt(5020, 4, 20);
  log.refreshStart(5100);
  log.pageVisible(5600);
  EXPECT_EQ(log.lastChapter().totalMs, 4600u);
  EXPECT_EQ(log.lastChapter().section.buildMs, 4000u);
}

TEST(SpeedCheckOpen, TheOpensOwnChapterIsNotAChapterChange) {
  Log log;
  log.beginOpen();
  log.sectionEntry(500, 2);
  log.sectionProbed(false);
  log.refreshStart(560);
  log.pageVisible(1400);
  OpenRecord rec;
  rec.loadMs = 210;
  rec.setupMs = 95;
  rec.sectionMs = 35;
  rec.drawMs = 900;
  rec.totalMs = 1240;
  log.bookOpened(rec);

  EXPECT_FALSE(log.lastChapter().valid);
  const OpenRecord& open = log.lastOpen();
  ASSERT_TRUE(open.valid);
  EXPECT_EQ(open.totalMs, 1240u);
  EXPECT_EQ(open.section.source, Source::Cached);
  EXPECT_EQ(open.section.spine, 2);

  // The next section entry is a chapter change again.
  log.sectionEntry(9000, 3);
  log.sectionProbed(false);
  log.refreshStart(9050);
  log.pageVisible(9600);
  EXPECT_TRUE(log.lastChapter().valid);
}

TEST(SpeedCheckOpen, ALiveBuildCompletesTheOpenRecord) {
  Log log;
  log.beginOpen();
  log.sectionEntry(500, 0);
  log.sectionProbed(true);
  log.refreshStart(1500);
  log.pageVisible(2100);
  OpenRecord rec;
  rec.totalMs = 1900;
  log.bookOpened(rec);
  EXPECT_EQ(log.lastOpen().section.source, Source::BuiltLive);
  EXPECT_FALSE(log.lastOpen().section.buildDone);
  log.sectionBuilt(4500, 0, 22);
  EXPECT_TRUE(log.lastOpen().section.buildDone);
  EXPECT_EQ(log.lastOpen().section.buildMs, 4000u);
  EXPECT_EQ(log.lastOpen().section.pages, 22);
}

TEST(SpeedCheckOpen, ASecondSummaryOfTheSameOpenIsIgnored) {
  Log log;
  log.beginOpen();
  OpenRecord rec;
  rec.totalMs = 1000;
  log.bookOpened(rec);
  rec.totalMs = 5;
  log.bookOpened(rec);
  EXPECT_EQ(log.lastOpen().totalMs, 1000u);
}

TEST(SpeedCheckOpen, AnOpenDropsATurnThatWasInFlight) {
  Log log;
  log.noteInput(1000, 1010, false);
  log.beginOpen();
  log.refreshStart(1500);
  log.pageVisible(2000);
  EXPECT_EQ(log.turnCount(), 0);
}

ReportEnv testEnv() {
  ReportEnv env;
  env.firmware = "2.41-test";
  env.board = "xteink_x4";
  env.settings = "clean-refresh-every=15 pages";
  env.uptimeSeconds = 321;
  env.cpuMhz = 160;
  env.cpuIdleMhz = 10;
  env.idleDownclockMs = 500;
  env.lightSleepAfterMs = 1000;
  env.lightSleepSliceMs = 50;
  env.lightSleepSeconds = 200;
  env.lightSleepSlices = 4000;
  env.lightSleepDeclined = 17;
  env.usbPower = true;
  env.batteryPercent = 83;
  env.freeHeap = 91234;
  env.sdTotalMb = 29800;
  env.sdFreeMb = 21000;
  return env;
}

TEST(SpeedCheckReport, AnEmptyLogSaysSoAndStillDescribesTheDevice) {
  Log log;
  char out[3072];
  const size_t n = formatReport(log, testEnv(), out, sizeof(out));
  const std::string text(out, n);
  EXPECT_EQ(n, strlen(out));
  EXPECT_NE(text.find("firmware: 2.41-test\n"), std::string::npos);
  EXPECT_NE(text.find("board: xteink_x4\n"), std::string::npos);
  EXPECT_NE(text.find("BOOK OPEN (last)\n  none measured"), std::string::npos);
  EXPECT_NE(text.find("CHAPTER CHANGE (last)\n  none measured"), std::string::npos);
  EXPECT_NE(text.find("PAGE TURNS (last 0)\n  none measured"), std::string::npos);
  EXPECT_NE(text.find("cpu now: 160 MHz (idle: 10 MHz after 500 ms, light sleep after 1000 ms in 50 ms slices)"),
            std::string::npos);
  EXPECT_NE(text.find("usb power: yes, battery 83%"), std::string::npos);
  EXPECT_NE(text.find("size 29800 MB, free 21000 MB\n"), std::string::npos);
  EXPECT_EQ(text.back(), '\n');
}

TEST(SpeedCheckReport, AFullLogIsWrittenOutAndFitsItsBuffer) {
  Log log;
  log.beginOpen();
  log.sectionEntry(500, 2);
  log.sectionProbed(true);
  log.sectionBuilt(2900, 2, 14);
  log.refreshStart(3000);
  log.pageVisible(3900);
  OpenRecord rec;
  rec.fromWake = true;
  rec.loadMs = 210;
  rec.setupMs = 95;
  rec.sectionMs = 2400;
  rec.drawMs = 900;
  rec.totalMs = 3605;
  rec.sinceBootMs = 5100;
  log.bookOpened(rec);

  log.noteInput(10000, 10015, false);
  log.sectionEntry(10030, 3);
  log.sectionProbed(false);
  log.refreshStart(10100);
  log.pageVisible(10650);

  uint32_t now = 20000;
  for (int i = 0; i < kTurnCapacity; i++) {
    // The worst case for the buffer: every figure five digits wide.
    turn(log, now, now + 30000, now + 60000, now + 99000, /*preDrawn=*/true);
    now += 100000;
  }

  char out[kReportBytes];
  const size_t n = formatReport(log, testEnv(), out, sizeof(out));
  const std::string text(out, n);
  EXPECT_NE(text.find("  total: 3605 ms (open request to first page settled)\n"), std::string::npos);
  EXPECT_NE(text.find("  load book: 210 ms\n"), std::string::npos);
  EXPECT_NE(text.find("  chapter: 2400 ms (built in 2400 ms, 14 pages)\n"), std::string::npos);
  EXPECT_NE(text.find("  first page draw: 900 ms\n"), std::string::npos);
  EXPECT_NE(text.find("  after sleep: yes, 5100 ms from power-on to the page\n"), std::string::npos);
  EXPECT_NE(text.find("  to first page: 650 ms, spine 3, cached\n"), std::string::npos);
  EXPECT_NE(text.find("PAGE TURNS (last 32)\n"), std::string::npos);
  EXPECT_NE(text.find("  screen refresh: median 39000, min 39000, max 39000 ms\n"), std::string::npos);
  EXPECT_NE(text.find("  pages drawn ahead of the turn: 32 of 32\n"), std::string::npos);
  EXPECT_NE(text.find(" 30000/30000/39000*"), std::string::npos);
  // Everything fitted: the last section is there, whole.
  EXPECT_NE(text.find("SD CARD\n  size 29800 MB, free 21000 MB\n"), std::string::npos);
  EXPECT_LT(n, sizeof(out) - 64);  // and room is left for the save's own closing line
}

TEST(SpeedCheckReport, ALiveBuildsDrawIsReportedInsideItsChapter) {
  Log log;
  log.beginOpen();
  log.sectionEntry(500, 0);
  log.sectionProbed(true);
  log.refreshStart(1500);
  log.pageVisible(2100);
  OpenRecord rec;
  rec.sectionMs = 1600;
  rec.totalMs = 1900;
  log.bookOpened(rec);
  char out[3072];
  const std::string text(out, formatReport(log, testEnv(), out, sizeof(out)));
  EXPECT_NE(text.find("  chapter: 1600 ms (shown while building, build not finished)\n"), std::string::npos);
  EXPECT_NE(text.find("  first page draw: included in chapter\n"), std::string::npos);
  EXPECT_NE(text.find("  after sleep: no\n"), std::string::npos);
}

TEST(SpeedCheckReport, ASmallBufferGetsWholeLinesAndATerminator) {
  Log log;
  char out[80];
  memset(out, 'x', sizeof(out));
  const size_t n = formatReport(log, testEnv(), out, sizeof(out));
  ASSERT_LT(n, sizeof(out));
  EXPECT_EQ(out[n], '\0');
  EXPECT_EQ(strlen(out), n);
  EXPECT_EQ(std::string(out, n).find("x4-orbit speed report\n"), 0u);
}

TEST(SpeedCheckReport, DescribesEachWayAChapterWasHad) {
  char out[72];
  SectionTiming s;
  describeSection(s, out, sizeof(out));
  EXPECT_STREQ(out, "unknown");
  s.source = Source::Cached;
  describeSection(s, out, sizeof(out));
  EXPECT_STREQ(out, "cached");
  s.source = Source::BuiltLive;
  s.buildDone = true;
  s.buildMs = 5200;
  s.pages = 31;
  describeSection(s, out, sizeof(out));
  EXPECT_STREQ(out, "shown while building, build 5200 ms, 31 pages");
}

// ---- images ------------------------------------------------------------------------------

ImageDraw imageDraw(const char* name, ImageHow how, uint32_t ms, ImageFormat format = ImageFormat::BaselineJpeg) {
  ImageDraw d;
  setImageName(d, name);
  d.how = how;
  d.ms = ms;
  d.format = format;
  d.outWidth = 400;
  d.outHeight = 600;
  if (how != ImageHow::CacheHit) {
    d.sourceBytes = 300 * 1024;
    d.srcWidth = 1600;
    d.srcHeight = 2400;
  }
  return d;
}

TEST(SpeedCheckImages, ShortensALongNameInTheMiddle) {
  ImageDraw d;
  setImageName(d, "cover.jpg");
  EXPECT_STREQ(d.name, "cover.jpg");
  setImageName(d, "chapter-01-illustration-large.jpeg");
  EXPECT_EQ(strlen(d.name), kImageNameBytes - 1);
  EXPECT_STREQ(d.name, "chapter-0~arge.jpeg");
  setImageName(d, "exactly-19-chars.ab");  // 19: fits whole
  EXPECT_STREQ(d.name, "exactly-19-chars.ab");
}

TEST(SpeedCheckImages, KeepsTheLastSixteenOldestFirstAndTotalsEveryKind) {
  Log log;
  for (int i = 0; i < 20; i++) {
    char name[16];
    snprintf(name, sizeof(name), "img%02d.jpg", i);
    log.imageDrawn(imageDraw(name, i % 2 ? ImageHow::Background : ImageHow::PageTurn, 100));
  }
  ASSERT_EQ(log.imageCount(), kImageCapacity);
  EXPECT_STREQ(log.image(0).name, "img04.jpg");
  EXPECT_STREQ(log.image(kImageCapacity - 1).name, "img19.jpg");
  EXPECT_EQ(log.imageTotals(ImageHow::PageTurn).count, 10u);
  EXPECT_EQ(log.imageTotals(ImageHow::Background).count, 10u);
  EXPECT_EQ(log.imageTotals(ImageHow::Background).ms, 1000u);
  EXPECT_EQ(log.imageTotals(ImageHow::CacheHit).count, 0u);
}

TEST(SpeedCheckImages, ACacheHitIsListedOncePerImageButAlwaysCounted) {
  Log log;
  log.imageDrawn(imageDraw("a.png", ImageHow::Background, 4000, ImageFormat::Png));
  for (int i = 0; i < 30; i++) log.imageDrawn(imageDraw("a.png", ImageHow::CacheHit, 20));
  ImageDraw grey = imageDraw("a.png", ImageHow::CacheHit, 30);
  grey.grey = true;
  log.imageDrawn(grey);
  log.imageDrawn(grey);
  EXPECT_EQ(log.imageCount(), 2);  // the decode (its 1-bit hits are that image again), one grey hit
  EXPECT_EQ(log.image(0).how, ImageHow::Background);
  EXPECT_EQ(log.image(1).how, ImageHow::CacheHit);
  EXPECT_TRUE(log.image(1).grey);
  EXPECT_EQ(log.imageTotals(ImageHow::CacheHit).count, 32u);
  EXPECT_EQ(log.imageTotals(ImageHow::CacheHit).ms, 660u);
}

TEST(SpeedCheckReport, ListsEachImageWithItsFormatSizesAndTime) {
  Log log;
  log.imageDrawn(imageDraw("map.jpg", ImageHow::PageTurn, 5321, ImageFormat::ProgressiveJpeg));
  ImageDraw failed = imageDraw("broken.gif", ImageHow::Background, 12, ImageFormat::Gif);
  failed.ok = false;
  log.imageDrawn(failed);
  ImageDraw inPlace = imageDraw("plate.png", ImageHow::Background, 800, ImageFormat::Png);
  inPlace.sourceBytes = 0;
  log.imageDrawn(inPlace);
  log.imageDrawn(imageDraw("map.jpg", ImageHow::CacheHit, 45));    // listed already: counted only
  log.imageDrawn(imageDraw("cover.jpg", ImageHow::CacheHit, 38));  // decoded before the list began
  char out[kReportBytes];
  const std::string report(out, formatReport(log, testEnv(), out, sizeof(out)));
  EXPECT_NE(report.find("IMAGES (last 4)\n"), std::string::npos);
  EXPECT_NE(report.find("  decoded on a page turn: 1, 5321 ms in all\n"), std::string::npos);
  EXPECT_NE(report.find("  decoded in the background: 2, 812 ms in all\n"), std::string::npos);
  EXPECT_NE(report.find("  drawn from the cache: 2, 83 ms in all\n"), std::string::npos);
  EXPECT_NE(report.find("   map.jpg: page turn, progressive JPEG, 300 KB, 1600x2400 -> 400x600, 5321 ms\n"),
            std::string::npos);
  EXPECT_NE(report.find("   broken.gif: background, GIF, 300 KB, 1600x2400 -> 400x600, 12 ms, FAILED\n"),
            std::string::npos);
  EXPECT_NE(report.find("   plate.png: background, PNG, ? KB,"), std::string::npos);
  EXPECT_NE(report.find("   cover.jpg: cache, 400x600, 38 ms\n"), std::string::npos);
  EXPECT_EQ(report.find("   map.jpg: cache"), std::string::npos);
  EXPECT_LT(report.find("IMAGES"), report.find("CPU AND POWER"));
}

TEST(SpeedCheckReport, AFullReportFitsTheDeviceBuffer) {
  Log log;
  for (int i = 0; i < 40; i++) {
    log.noteInput(1000u * i, 1000u * i + 10, i % 3 == 0);
    log.refreshStart(1000u * i + 400);
    log.pageVisible(1000u * i + 900);
  }
  for (int i = 0; i < 20; i++) {
    char name[40];
    snprintf(name, sizeof(name), "chapter-%02d-illustration-plate.jpeg", i);
    ImageDraw d = imageDraw(name, ImageHow::Background, 65000, ImageFormat::ProgressiveJpeg);
    d.grey = true;
    d.ok = false;
    log.imageDrawn(d);
  }
  char out[kReportBytes];
  ReportEnv env = testEnv();
  const size_t n = formatReport(log, env, out, sizeof(out));
  EXPECT_NE(std::string(out, n).find("SD CARD\n"), std::string::npos) << n;
}

}  // namespace
