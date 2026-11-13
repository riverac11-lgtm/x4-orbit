#include "OrbitProfile.h"

#include "CrossPointSettings.h"

void OrbitProfile::apply(CrossPointSettings& s) {
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
