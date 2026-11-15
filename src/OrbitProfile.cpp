#include "OrbitProfile.h"

#include "CrossPointSettings.h"
#include "components/themes/PanelMetrics.h"
#include "util/ReadingCalendar.h"

void OrbitProfile::apply(CrossPointSettings& s, const uint8_t fromVersion) {
  if (fromVersion < 3) {
    // Version 3: a daily goal gives Reading Stats' ring and calendar something to measure against,
    // and a reader who turns on a book progress bar sees where the chapters fall on it. Both only
    // over the upstream "off", which is also what a card from before these settings reads as.
    if (s.readingGoal == 0) s.readingGoal = ReadingCalendar::kGoalDefaultChoice;
    s.statusBarChapterMarks = 1;
  }
  if (fromVersion < 2) {
    // Version 2: the Orbit-A home (a continue-reading card over four large tiles) and the calmer
    // chrome that comes with it -- but only over the upstream default, never over a theme chosen.
    // A compact panel offers no Orbit theme (SettingsList.h uiThemeChoices).
    if (!PanelMetrics::COMPACT && s.uiTheme == CrossPointSettings::LYRA) s.uiTheme = CrossPointSettings::ORBIT_A;
  }
  if (fromVersion >= 1) return;

  // Home: the two things a reader does there, then More and Settings.
  s.showLibraryOnHome = 1;
  s.showOnlineLibraryOnHome = 1;
  s.showReadingStatsOnHome = 0;
  s.showBookmarksOnHome = 0;
  s.showFileTransferOnHome = 0;

  // Front Left/Right page turns: upstream gives them a double-press (jump 10 pages), which makes
  // every single press wait 300 ms to see whether a second follows. Speed of an ordinary turn
  // matters more here; the jump stays one setting away (Controls > Button Actions).
  if (s.btnDoubleLeft == CrossPointSettings::BTN_PAGE_BACK_10) s.btnDoubleLeft = CrossPointSettings::BTN_DEFAULT;
  if (s.btnDoubleRight == CrossPointSettings::BTN_PAGE_FORWARD_10) {
    s.btnDoubleRight = CrossPointSettings::BTN_DEFAULT;
  }

  // Asleep, show the book being read with its title and progress; with no book open this falls
  // back to the default screen, which is the orbit picture.
  if (s.sleepScreen == CrossPointSettings::DARK) {
    s.sleepScreen = CrossPointSettings::COVER;
    if (s.sleepCoverOverlay == 0) s.sleepCoverOverlay = 1;
  }
}
