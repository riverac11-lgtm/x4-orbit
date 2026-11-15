#pragma once

#include "ReadingStats.h"
#include "activities/Activity.h"

// Phase-1 reading-stats screen. Read-only summary of the per-book and global
// reading counters collected by ReadingSessionTracker. Mirrors the layout of
// SystemInformationActivity so it slots into the existing settings flow with
// no new theme work. Future phases will add per-book drill-in, day-by-day
// sparklines, and a web-frontend dashboard.
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
  std::vector<BookReadingStats> topBooks_;  // up to three, most time first
  void loadSummary();

  // The goal ring and the month calendar. `today_` is 0 while the clock has never been set: no
  // ring then. The calendar needs a clock that is not merely a stale restore (calendar_).
  uint16_t today_ = 0;
  bool calendar_ = false;
  int year_ = 0, month_ = 0;            // the month shown
  int nowYear_ = 0, nowMonth_ = 0;      // today's month: the latest one Right reaches
  int firstYear_ = 0, firstMonth_ = 0;  // the oldest month the history holds: the earliest Left reaches
  bool canShowMonth(int delta) const;

  // Last millis() at which we refreshed the live "this session" block.
  // We only redraw once per second to avoid hammering the e-ink panel.
  uint32_t lastLiveRefreshMs = 0;
};
