// Cover for where each home screen entry ends up: on the home screen, behind its "More" entry,
// or nowhere. The one promise the "More" entry makes is that hiding an entry never makes it
// unreachable -- and, the other way round, that a More row never opens onto an empty list.
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "CrossPointSettings.h"
#include "OrbitProfile.h"
#include "activities/home/HomeMenu.h"
#include "activities/home/LibraryTab.h"

CrossPointSettings CrossPointSettings::instance;

namespace {

using A = HomeMenuAction;

std::vector<A> actionsAt(const HomeMenuPlacement placement, const HomeMenuAvailability& availability) {
  std::vector<HomeMenuEntry> entries;
  collectHomeMenuEntries(placement, availability, entries);
  std::vector<A> actions;
  for (const HomeMenuEntry& entry : entries) actions.push_back(entry.action);
  return actions;
}

class HomeMenuTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SETTINGS.showLibraryOnHome = 1;
    SETTINGS.showOnlineLibraryOnHome = 1;
    SETTINGS.showReadingStatsOnHome = 1;
    SETTINGS.showBookmarksOnHome = 1;
    SETTINGS.showFileTransferOnHome = 1;
    SETTINGS.showWeatherOnHome = 1;
    SETTINGS.useWeather = 1;
  }

  HomeMenuAvailability everything{.hasBookmarks = true};
  HomeMenuAvailability nothingOptional{.hasBookmarks = false};
};

TEST_F(HomeMenuTest, DefaultsShowEverythingAvailableAndNoMore) {
  EXPECT_EQ(
      actionsAt(HomeMenuPlacement::Home, everything),
      (std::vector<A>{A::Library, A::ReadingStats, A::GlobalBookmarks, A::FileTransfer, A::Weather, A::Settings}));
  EXPECT_TRUE(actionsAt(HomeMenuPlacement::More, everything).empty());
}

TEST_F(HomeMenuTest, EntriesWithNothingBehindThemAppearNowhere) {
  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, nothingOptional),
            (std::vector<A>{A::Library, A::ReadingStats, A::FileTransfer, A::Weather, A::Settings}));
}

TEST_F(HomeMenuTest, AHiddenEntryMovesBehindMoreWhichSitsAboveSettings) {
  SETTINGS.showFileTransferOnHome = 0;

  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, everything),
            (std::vector<A>{A::Library, A::ReadingStats, A::GlobalBookmarks, A::Weather, A::More, A::Settings}));
  EXPECT_EQ(actionsAt(HomeMenuPlacement::More, everything), (std::vector<A>{A::FileTransfer}));
}

TEST_F(HomeMenuTest, MoreKeepsTheHomeScreenOrder) {
  SETTINGS.showWeatherOnHome = 0;
  SETTINGS.showLibraryOnHome = 0;
  SETTINGS.showFileTransferOnHome = 0;

  EXPECT_EQ(actionsAt(HomeMenuPlacement::More, everything), (std::vector<A>{A::Library, A::FileTransfer, A::Weather}));
}

// Hiding Bookmarks with no bookmark saved hides nothing the user could open, so a More row here
// would lead to an empty list.
TEST_F(HomeMenuTest, HidingAnUnavailableEntryAddsNoMore) {
  SETTINGS.showBookmarksOnHome = 0;

  const auto home = actionsAt(HomeMenuPlacement::Home, nothingOptional);
  EXPECT_EQ(std::count(home.begin(), home.end(), A::More), 0);
  EXPECT_TRUE(actionsAt(HomeMenuPlacement::More, nothingOptional).empty());
}

TEST_F(HomeMenuTest, WeatherTurnedOffIsInNeitherPlace) {
  SETTINGS.useWeather = 0;
  SETTINGS.showWeatherOnHome = 0;

  const auto home = actionsAt(HomeMenuPlacement::Home, everything);
  EXPECT_EQ(std::count(home.begin(), home.end(), A::Weather), 0);
  EXPECT_EQ(std::count(home.begin(), home.end(), A::More), 0);
  EXPECT_TRUE(actionsAt(HomeMenuPlacement::More, everything).empty());
}

TEST_F(HomeMenuTest, SettingsStaysWhenEverythingElseIsHidden) {
  SETTINGS.showLibraryOnHome = 0;
  SETTINGS.showReadingStatsOnHome = 0;
  SETTINGS.showBookmarksOnHome = 0;
  SETTINGS.showFileTransferOnHome = 0;
  SETTINGS.showWeatherOnHome = 0;

  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, everything), (std::vector<A>{A::More, A::Settings}));
  EXPECT_EQ(actionsAt(HomeMenuPlacement::More, everything),
            (std::vector<A>{A::Library, A::ReadingStats, A::GlobalBookmarks, A::FileTransfer, A::Weather}));
}

// Every entry is reachable from exactly one place, whatever the settings say.
TEST_F(HomeMenuTest, NoEntryIsInBothPlaces) {
  SETTINGS.showLibraryOnHome = 0;
  SETTINGS.showWeatherOnHome = 0;

  const auto home = actionsAt(HomeMenuPlacement::Home, everything);
  for (const A action : actionsAt(HomeMenuPlacement::More, everything)) {
    EXPECT_EQ(std::count(home.begin(), home.end(), action), 0) << static_cast<int>(action);
  }
}

// Browse Files and Recent Books became the Library's tabs: one entry, under its own name.
TEST_F(HomeMenuTest, OneLibraryEntryLabelledLibrary) {
  std::vector<HomeMenuEntry> entries;
  collectHomeMenuEntries(HomeMenuPlacement::Home, everything, entries);
  ASSERT_FALSE(entries.empty());
  EXPECT_EQ(entries.front().action, A::Library);
  EXPECT_EQ(entries.front().label, StrId::STR_LIBRARY);
  EXPECT_EQ(entries.front().icon, Folder);
}

// Browse Files and Recent Books had a home entry each; the Library is shown where either was.
TEST(HomeMenuMigration, LibraryShownWhereEitherOldEntryWas) {
  EXPECT_EQ(libraryOnHomeFromLegacy(1, 1), 1);
  EXPECT_EQ(libraryOnHomeFromLegacy(1, 0), 1);
  EXPECT_EQ(libraryOnHomeFromLegacy(0, 1), 1);
  EXPECT_EQ(libraryOnHomeFromLegacy(0, 0), 0);
}

TEST(LibraryTabs, StoredValueOutOfRangeOpensBooks) {
  EXPECT_EQ(libraryTabFrom(LIBRARY_TAB_COUNT + 1), LibraryTab::Books);
  EXPECT_EQ(libraryTabFrom(255), LibraryTab::Books);
}

// The stored value keeps its meaning across the change: a card left on Authors (3) reopens Authors,
// and one left on the retired New tab (2) opens Recent, its nearest neighbour.
TEST(LibraryTabs, StoredValuesKeepTheirMeaningAndNewOpensRecent) {
  EXPECT_EQ(libraryTabFrom(0), LibraryTab::Books);
  EXPECT_EQ(libraryTabFrom(1), LibraryTab::Recent);
  EXPECT_EQ(libraryTabFrom(2), LibraryTab::Recent);
  EXPECT_EQ(libraryTabFrom(3), LibraryTab::Authors);
  EXPECT_EQ(libraryTabFrom(4), LibraryTab::Bookmarks);
  for (const LibraryTab tab : kLibraryTabOrder) EXPECT_EQ(libraryTabFrom(static_cast<uint8_t>(tab)), tab);
}

TEST(LibraryTabs, BarOrderIsBooksRecentAuthorsBookmarks) {
  EXPECT_EQ(libraryTabSlot(LibraryTab::Books), 0);
  EXPECT_EQ(libraryTabSlot(LibraryTab::Recent), 1);
  EXPECT_EQ(libraryTabSlot(LibraryTab::Authors), 2);
  EXPECT_EQ(libraryTabSlot(LibraryTab::Bookmarks), 3);
}

TEST(LibraryTabs, StepWrapsAtBothEnds) {
  EXPECT_EQ(libraryTabStep(LibraryTab::Books, 1), LibraryTab::Recent);
  EXPECT_EQ(libraryTabStep(LibraryTab::Recent, 1), LibraryTab::Authors);
  EXPECT_EQ(libraryTabStep(LibraryTab::Authors, 1), LibraryTab::Bookmarks);
  EXPECT_EQ(libraryTabStep(LibraryTab::Bookmarks, 1), LibraryTab::Books);
  EXPECT_EQ(libraryTabStep(LibraryTab::Books, -1), LibraryTab::Bookmarks);
}

// The bar's focus, as in Settings: Left/Right change tab, Down or Confirm go back to the list, Up
// stays; Up leaves the list for the bar only from its first row.
TEST(LibraryTabs, TabBarFocus) {
  EXPECT_EQ(libraryBarMove(true, false, false, false), LibraryBarMove::PrevTab);
  EXPECT_EQ(libraryBarMove(false, true, false, false), LibraryBarMove::NextTab);
  EXPECT_EQ(libraryBarMove(false, false, true, false), LibraryBarMove::ToList);
  EXPECT_EQ(libraryBarMove(false, false, false, true), LibraryBarMove::ToList);
  EXPECT_EQ(libraryBarMove(false, false, false, false), LibraryBarMove::Stay);
  EXPECT_TRUE(libraryUpToBar(0, 0));
  EXPECT_FALSE(libraryUpToBar(3, 0));
  EXPECT_TRUE(libraryUpToBar(1, 1));  // Bookmarks: row 0 is a book's title, not a stop
  EXPECT_FALSE(libraryUpToBar(2, 1));
}

}  // namespace

// x4-orbit: the Online Library entry and the simple home screen of the orbit profile.

TEST_F(HomeMenuTest, OnlineLibrarySitsRightAfterTheLibraryWhenOffered) {
  HomeMenuAvailability withCatalog{.hasBookmarks = true, .hasOnlineLibrary = true};
  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, withCatalog),
            (std::vector<A>{A::Library, A::OnlineLibrary, A::ReadingStats, A::GlobalBookmarks, A::FileTransfer,
                            A::Weather, A::Settings}));
}

TEST_F(HomeMenuTest, OnlineLibraryAppearsNowhereWhenTheBuildDoesNotOfferIt) {
  for (const auto placement : {HomeMenuPlacement::Home, HomeMenuPlacement::More}) {
    for (const A action : actionsAt(placement, everything)) EXPECT_NE(action, A::OnlineLibrary);
  }
}

TEST_F(HomeMenuTest, HidingOnlineLibraryMovesItBehindMore) {
  SETTINGS.showOnlineLibraryOnHome = 0;
  HomeMenuAvailability withCatalog{.hasBookmarks = true, .hasOnlineLibrary = true};
  EXPECT_EQ(actionsAt(HomeMenuPlacement::More, withCatalog), (std::vector<A>{A::OnlineLibrary}));
  SETTINGS.showOnlineLibraryOnHome = 1;
}

TEST_F(HomeMenuTest, OrbitProfileHomeHasNoMoreAndReadingStatsInItsPlace) {
  SETTINGS.useWeather = 0;  // the orbit build has no weather
  OrbitProfile::apply(SETTINGS);
  HomeMenuAvailability orbit{.hasBookmarks = true, .hasOnlineLibrary = true, .orbitHome = true};

  // Orbit-A's four tiles: Library, Online Library, the stats graphic, Settings.
  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, orbit),
            (std::vector<A>{A::Library, A::OnlineLibrary, A::ReadingStats, A::Settings}));
  EXPECT_TRUE(actionsAt(HomeMenuPlacement::More, orbit).empty());
}

// Bookmarks (a Library tab) and File Transfer (Settings > System) are reached elsewhere, so their old
// "show on home" settings put them nowhere, and Reading Stats stays on the home screen either way.
TEST_F(HomeMenuTest, OrbitHomeIgnoresTheOldPlacementOfStatsBookmarksAndTransfer) {
  SETTINGS.useWeather = 0;
  HomeMenuAvailability orbit{.hasBookmarks = true, .hasOnlineLibrary = true, .orbitHome = true};
  for (const uint8_t shown : {0, 1}) {
    SETTINGS.showReadingStatsOnHome = shown;
    SETTINGS.showBookmarksOnHome = shown;
    SETTINGS.showFileTransferOnHome = shown;
    EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, orbit),
              (std::vector<A>{A::Library, A::OnlineLibrary, A::ReadingStats, A::Settings}));
    EXPECT_TRUE(actionsAt(HomeMenuPlacement::More, orbit).empty());
  }
}

// The one way More comes back: the owner hid an entry that has no other way in.
TEST_F(HomeMenuTest, OrbitHomeKeepsMoreAsTheSafetyNetForAHiddenLibrary) {
  SETTINGS.useWeather = 0;
  SETTINGS.showLibraryOnHome = 0;
  HomeMenuAvailability orbit{.hasBookmarks = true, .hasOnlineLibrary = true, .orbitHome = true};
  EXPECT_EQ(actionsAt(HomeMenuPlacement::Home, orbit),
            (std::vector<A>{A::OnlineLibrary, A::ReadingStats, A::More, A::Settings}));
  EXPECT_EQ(actionsAt(HomeMenuPlacement::More, orbit), (std::vector<A>{A::Library}));
}

TEST_F(HomeMenuTest, OrbitProfileFourPutsHiddenLibraryEntriesBackOnHome) {
  SETTINGS.showLibraryOnHome = 0;
  SETTINGS.showOnlineLibraryOnHome = 0;
  SETTINGS.sleepScreen = CrossPointSettings::DARK;
  OrbitProfile::apply(SETTINGS, /*fromVersion=*/3);
  EXPECT_EQ(SETTINGS.showLibraryOnHome, 1);
  EXPECT_EQ(SETTINGS.showOnlineLibraryOnHome, 1);
  EXPECT_EQ(SETTINGS.sleepScreen, CrossPointSettings::DARK);  // version 1's step does not run again

  // Hidden again on a version-4 file, it stays hidden.
  SETTINGS.showLibraryOnHome = 0;
  OrbitProfile::apply(SETTINGS, OrbitProfile::VERSION);
  EXPECT_EQ(SETTINGS.showLibraryOnHome, 0);
  SETTINGS.showLibraryOnHome = 1;
}

TEST_F(HomeMenuTest, OrbitProfileMovesTheDefaultSleepScreenToTheBookCoverButKeepsAnOwnChoice) {
  SETTINGS.sleepScreen = CrossPointSettings::DARK;
  SETTINGS.sleepCoverOverlay = 0;
  OrbitProfile::apply(SETTINGS);
  EXPECT_EQ(SETTINGS.sleepScreen, CrossPointSettings::COVER);
  EXPECT_NE(SETTINGS.sleepCoverOverlay, 0);

  SETTINGS.sleepScreen = CrossPointSettings::LIGHT;
  OrbitProfile::apply(SETTINGS);
  EXPECT_EQ(SETTINGS.sleepScreen, CrossPointSettings::LIGHT);
}

TEST_F(HomeMenuTest, OrbitProfileDropsTheDoublePressWaitOnlyWhereItIsStillTheDefault) {
  SETTINGS.btnDoubleLeft = CrossPointSettings::BTN_PAGE_BACK_10;
  SETTINGS.btnDoubleRight = CrossPointSettings::BTN_PAGE_BACK_10;  // the owner's own choice
  OrbitProfile::apply(SETTINGS);
  EXPECT_EQ(SETTINGS.btnDoubleLeft, CrossPointSettings::BTN_DEFAULT);
  EXPECT_EQ(SETTINGS.btnDoubleRight, CrossPointSettings::BTN_PAGE_BACK_10);
}

TEST_F(HomeMenuTest, OrbitProfileTwoMovesTheDefaultThemeToOrbitAButKeepsAnOwnChoice) {
  SETTINGS.uiTheme = CrossPointSettings::LYRA;
  OrbitProfile::apply(SETTINGS);
  EXPECT_EQ(SETTINGS.uiTheme, CrossPointSettings::ORBIT_A);

  SETTINGS.uiTheme = CrossPointSettings::LYRA_CAROUSEL;
  OrbitProfile::apply(SETTINGS);
  EXPECT_EQ(SETTINGS.uiTheme, CrossPointSettings::LYRA_CAROUSEL);
  SETTINGS.uiTheme = CrossPointSettings::LYRA;
}

TEST_F(HomeMenuTest, OrbitProfileTwoOverAVersionOneFileLeavesItsHomeEntriesAlone) {
  SETTINGS.uiTheme = CrossPointSettings::LYRA;
  SETTINGS.showReadingStatsOnHome = 1;  // chosen by the owner after version 1
  SETTINGS.sleepScreen = CrossPointSettings::DARK;
  OrbitProfile::apply(SETTINGS, /*fromVersion=*/1);
  EXPECT_EQ(SETTINGS.uiTheme, CrossPointSettings::ORBIT_A);
  EXPECT_EQ(SETTINGS.showReadingStatsOnHome, 1);
  EXPECT_EQ(SETTINGS.sleepScreen, CrossPointSettings::DARK);
  SETTINGS.uiTheme = CrossPointSettings::LYRA;
  SETTINGS.showReadingStatsOnHome = 0;

  // An up-to-date file gets nothing.
  SETTINGS.uiTheme = CrossPointSettings::LYRA;
  OrbitProfile::apply(SETTINGS, OrbitProfile::VERSION);
  EXPECT_EQ(SETTINGS.uiTheme, CrossPointSettings::LYRA);
}

TEST_F(HomeMenuTest, OrbitProfileThreeSetsChapterMarksOnce) {
  SETTINGS.statusBarChapterMarks = 0;
  OrbitProfile::apply(SETTINGS, /*fromVersion=*/2);
  EXPECT_EQ(SETTINGS.statusBarChapterMarks, 1);

  // Turned off on a version-3 file, they stay off.
  SETTINGS.statusBarChapterMarks = 0;
  OrbitProfile::apply(SETTINGS, OrbitProfile::VERSION);
  EXPECT_EQ(SETTINGS.statusBarChapterMarks, 0);
}

TEST_F(HomeMenuTest, OrbitProfileV3GivesTheChapterMarksABookBarUnlessOneIsChosen) {
  SETTINGS.statusBarUpperProgressBar = CrossPointSettings::HIDE_PROGRESS;
  SETTINGS.statusBarLowerProgressBar = CrossPointSettings::HIDE_PROGRESS;
  OrbitProfile::apply(SETTINGS, 2);
  EXPECT_EQ(SETTINGS.statusBarLowerProgressBar, CrossPointSettings::BOOK_PROGRESS);

  SETTINGS.statusBarLowerProgressBar = CrossPointSettings::HIDE_PROGRESS;
  SETTINGS.statusBarUpperProgressBar = CrossPointSettings::CHAPTER_PROGRESS;  // the owner's own bar
  OrbitProfile::apply(SETTINGS, 2);
  EXPECT_EQ(SETTINGS.statusBarLowerProgressBar, CrossPointSettings::HIDE_PROGRESS);
}
