#include "OrbitProfile.h"

#include "CrossPointSettings.h"

void OrbitProfile::apply(CrossPointSettings& s) {
  // Home: the two things a reader does there, then More and Settings.
  s.showLibraryOnHome = 1;
  s.showOnlineLibraryOnHome = 1;
  s.showReadingStatsOnHome = 0;
  s.showBookmarksOnHome = 0;
  s.showFileTransferOnHome = 0;

  // Asleep, show the book being read with its title and progress; with no book open this falls
  // back to the default screen, which is the orbit picture.
  if (s.sleepScreen == CrossPointSettings::DARK) {
    s.sleepScreen = CrossPointSettings::COVER;
    if (s.sleepCoverOverlay == 0) s.sleepCoverOverlay = 1;
  }
}
