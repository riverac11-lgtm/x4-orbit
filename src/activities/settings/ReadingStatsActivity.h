#pragma once

#include <vector>

#include "ReadingStats.h"
#include "activities/Activity.h"
#include "util/ReadingInsights.h"

// Reading Stats: the current book's ring and time left, the recent sessions, the milestones and the
// top books. Nothing on it goes by the date: the X4's clock stops while it sleeps (Use Clock off),
// so a calendar or a "today" would be wrong. Confirm opens the all-books list.
class ReadingStatsActivity final : public Activity {
 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // What the screen draws, read once on entry (and again when the list may have removed books).
  // Written on the loop task under RenderLock; read by render().
  ReadingStatsStore::Summary summary_;
  std::vector<BookReadingStats> topBooks_;  // up to three, duplicates merged, most time first
  ReadingInsights::SessionLog sessions_;
  BookReadingStats current_;    // the book last read; empty docId: none to show
  bool lastIsCurrent_ = false;  // the newest logged session was of current_
  void loadSummary();
};
