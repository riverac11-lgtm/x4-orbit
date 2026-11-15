#include "ReadingStatsActivity.h"

#include <Arduino.h>  // millis()
#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReadingSessionTracker.h"
#include "ReadingStats.h"
#include "ReadingStatsBookListActivity.h"
#include "components/BookProgressPresentation.h"
#include "components/CardLayout.h"
#include "components/StatsDraw.h"
#include "components/UITheme.h"
#include "components/themes/PanelMetrics.h"
#include "fontIds.h"
#include "util/ReadingCalendar.h"

namespace {

// "1h 23m" / "23m 45s" / "12s" — compact so a long row fits on the X3.
using BookProgressPresentation::formatReadingDuration;

// Pages per minute, rounded to 1 decimal. Returns "—" when there's not enough
// data (fewer than a minute total) so we don't display "120.0 ppm" when only
// a handful of seconds have been recorded.
std::string formatPagesPerMin(uint32_t pages, uint32_t seconds) {
  if (seconds < 60 || pages == 0) {
    return tr(STR_READING_STATS_UNKNOWN);
  }
  const float ppm = (pages * 60.0f) / seconds;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", ppm);
  return buf;
}

}  // namespace

void ReadingStatsActivity::loadSummary() {
  ReadingStatsStore::Summary summary;
  std::vector<BookReadingStats> top;
  const bool ok = READING_STATS.querySummary(summary, /*withIndex=*/true) == ReadingStatsStore::ReadResult::Ok;
  if (ok) {
    const size_t shown = std::min<size_t>(summary.byTime.size(), 3);
    if (READING_STATS.queryBooksAt(summary.byTime, 0, shown, summary.seq, top) != ReadingStatsStore::ReadResult::Ok) {
      top.clear();
    }
    for (BookReadingStats& book : top) book.days.clear();  // the card shows title and time only
    std::vector<ReadingStatsStore::IndexEntry>().swap(summary.byTime);
  }
  RenderLock lock(*this);
  summary_ = std::move(summary);
  topBooks_ = std::move(top);
}

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  loadSummary();
  today_ = currentLocalDayIndex();
  // A clock restored from a stale save knows roughly the date, enough for "today" and the streak,
  // but a calendar would show the wrong day ringed.
  calendar_ = today_ != 0 && !HalClock::isStaleRestore();
  if (today_ != 0) {
    const ReadingCalendar::Date now = ReadingCalendar::dateOf(today_);
    nowYear_ = year_ = firstYear_ = now.year;
    nowMonth_ = month_ = firstMonth_ = now.month;
    if (!summary_.globalDays.empty() && summary_.globalDays.front().dayIndex < today_) {
      const ReadingCalendar::Date first = ReadingCalendar::dateOf(summary_.globalDays.front().dayIndex);
      firstYear_ = first.year;
      firstMonth_ = first.month;
    }
  }
  requestUpdate();
}

bool ReadingStatsActivity::canShowMonth(const int delta) const {
  if (!calendar_) return false;
  const int target = year_ * 12 + month_ + delta;
  return target >= firstYear_ * 12 + firstMonth_ && target <= nowYear_ * 12 + nowMonth_;
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  // Confirm opens the all-books list when there's anything to drill into.
  // Suppressed when the store is empty so the button hint never lies.
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && summary_.bookCount > 0) {
    startActivityForResult(std::make_unique<ReadingStatsBookListActivity>(renderer, mappedInput),
                           [this](const ActivityResult&) {
                             // The list may have removed books.
                             loadSummary();
                             requestUpdate();
                           });
    return;
  }
  // Left/Right page through the calendar's months: back to the oldest the history holds, forward to
  // this one.
  const int delta = mappedInput.wasPressed(MappedInputManager::Button::Left)    ? -1
                    : mappedInput.wasPressed(MappedInputManager::Button::Right) ? 1
                                                                                : 0;
  if (delta != 0 && canShowMonth(delta)) {
    {
      RenderLock lock(*this);
      ReadingCalendar::shiftMonth(year_, month_, delta);
    }
    requestUpdate();
    return;
  }
  // If a reading session happens to be live, tick at most once per second so
  // "this session" moves visibly without hammering the e-ink panel.
  if (globalReadingSessionTracker().isActive()) {
    const uint32_t now = millis();
    if (now - lastLiveRefreshMs >= 1000) {
      lastLiveRefreshMs = now;
      requestUpdate();
    }
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect contentRect = UITheme::getContentRect(renderer, /*hasBottomHints=*/true, /*hasSideHints=*/false);

  renderer.clearScreen();

  GUI.drawHeader(renderer,
                 Rect{contentRect.x, contentRect.y + metrics.topPadding, contentRect.width, metrics.headerHeight},
                 tr(STR_READING_STATS), nullptr);

  const int startY = contentRect.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  CardLayout::Config cfg;
  cfg.outerMarginX = PanelMetrics::capped(metrics.verticalSpacing * 2, 4);
  CardLayout layout(renderer, contentRect, startY, cfg);

  auto& tracker = globalReadingSessionTracker();
  // The cards do not page: a card that would run into the button hints is left out.
  const int bottom = contentRect.y + contentRect.height - metrics.verticalSpacing;
  const int rowStep = renderer.getLineHeight(UI_10_FONT_ID) + 2;
  const auto fits = [&](const bool titled, const int bodyHeight) {
    return layout.cursorY() + layout.cardHeight(titled, bodyHeight) <= bottom;
  };
  const uint8_t goal = ReadingCalendar::goalMinutes(SETTINGS.readingGoal);

  // ---- Today: the goal ring and the streak ----
  if (today_ != 0) {
    const uint32_t todaySeconds =
        ReadingStatsStore::secondsOn(summary_.globalDays, today_) + tracker.getUnrecordedSeconds();
    const int minutes = static_cast<int>(todaySeconds / 60);
    const uint16_t streak = ReadingStatsStore::currentStreakIn(summary_.globalDays, today_);
    char caption[24], line[40], streakText[32];
    StatsDraw::Today view;
    view.minutes = minutes;
    view.goalMinutes = goal;
    if (goal > 0) {
      snprintf(caption, sizeof(caption), tr(STR_GOAL_OF), goal);
      if (minutes >= goal) {
        snprintf(line, sizeof(line), "%s", tr(STR_GOAL_REACHED));
      } else {
        snprintf(line, sizeof(line), tr(STR_GOAL_TO_GO), goal - minutes);
      }
    } else {
      snprintf(caption, sizeof(caption), "%s", tr(STR_GOAL_MIN));
      snprintf(line, sizeof(line), tr(STR_GOAL_READ), minutes);
    }
    if (streak > 0) {
      snprintf(streakText, sizeof(streakText), tr(STR_GOAL_STREAK), streak);
    } else {
      snprintf(streakText, sizeof(streakText), "%s", tr(STR_GOAL_NO_STREAK));
    }
    view.ringCaption = caption;
    view.heading = tr(STR_GOAL_TODAY);
    view.line = line;
    view.streak = streakText;
    layout.card(nullptr, [&](CardLayout::Body& b) {
      b.advance(StatsDraw::drawToday(renderer, b.innerLeft(), b.currentY(), b.innerWidth(), view));
    });
  }

  // ---- The month calendar ----
  if (calendar_) {
    StatsDraw::Month view;
    char monthName[24], title[40];
    ReadingCalendar::listItem(tr(STR_CAL_MONTHS), month_ - 1, monthName, sizeof(monthName));
    snprintf(title, sizeof(title), "%s %d", monthName, year_);
    view.title = title;
    view.weekdays = tr(STR_CAL_WEEKDAYS);
    view.firstWeekday = ReadingCalendar::weekdayMondayFirst(ReadingCalendar::dayIndexOf(year_, month_, 1));
    view.dayCount = ReadingCalendar::daysInMonth(year_, month_);
    uint16_t minutes[31];
    ReadingCalendar::monthMinutes(summary_.globalDays, year_, month_, minutes);
    const ReadingCalendar::Date now = ReadingCalendar::dateOf(today_);
    const bool thisMonth = year_ == now.year && month_ == now.month;
    if (thisMonth) minutes[now.day - 1] += static_cast<uint16_t>(tracker.getUnrecordedSeconds() / 60);
    for (int d = 0; d < view.dayCount; ++d) view.shade[d] = ReadingCalendar::shade(minutes[d] * 60u, goal);
    view.today = thisMonth ? now.day : 0;
    view.lastDay = thisMonth ? now.day : view.dayCount;
    view.canGoBack = canShowMonth(-1);
    view.canGoForward = canShowMonth(1);
    layout.card(nullptr, [&](CardLayout::Body& b) {
      b.advance(StatsDraw::drawMonth(renderer, b.innerLeft(), b.currentY(), b.innerWidth(), view));
    });
  }

  // ---- Live session card ----
  if (tracker.isActive() && fits(true, rowStep * 2)) {
    layout.card(tr(STR_READING_STATS_CURRENT_SESSION), [&](CardLayout::Body& b) {
      b.rowLR(tr(STR_READING_STATS_TOTAL_TIME), formatReadingDuration(tracker.getLiveSeconds()));
      b.rowLR(tr(STR_READING_STATS_PAGES), std::to_string(tracker.getLivePages()));
    });
  }

  // ---- All-time card ----
  // The stat grid's height with two-line labels, and its rows: the most this card can take.
  const int gridHeight = rowStep + 2 * renderer.getLineHeight(SMALL_FONT_ID) + 4;
  if (fits(true, gridHeight + rowStep * 4))
    layout.card(tr(STR_READING_STATS_TOTAL_TIME), [&](CardLayout::Body& b) {
      if (summary_.totalSeconds == 0) {
        b.centeredMessage(tr(STR_READING_STATS_NO_DATA));
        return;
      }

      // 4-cell stat grid: sessions / books / current streak / longest streak.
      // Streaks read "—" when the clock has never been wall-anchored.
      const uint16_t today = currentLocalDayIndex();
      const bool haveStreak = today != 0 && !summary_.globalDays.empty();
      const std::string curStreak = haveStreak
                                        ? std::to_string(ReadingStatsStore::currentStreakIn(summary_.globalDays, today))
                                        : std::string(tr(STR_READING_STATS_UNKNOWN));
      const std::string maxStreak =
          haveStreak ? std::to_string(ReadingStatsStore::longestStreakIn(summary_.globalDays, summary_.longestStreak))
                     : std::string(tr(STR_READING_STATS_UNKNOWN));
      b.statGrid({{{std::to_string(summary_.totalSessions), tr(STR_READING_STATS_SESSIONS)},
                   {std::to_string(summary_.bookCount), tr(STR_READING_STATS_BOOKS)},
                   {curStreak, tr(STR_READING_STATS_STREAK)},
                   {maxStreak, tr(STR_READING_STATS_LONGEST)}}});

      b.rowLR(tr(STR_READING_STATS_TOTAL_TIME), formatReadingDuration(summary_.totalSeconds));
      b.rowLR(tr(STR_READING_STATS_PAGES), std::to_string(summary_.totalPagesTurned));
      b.rowLR(tr(STR_READING_STATS_PAGES_PER_MIN), formatPagesPerMin(summary_.totalPagesTurned, summary_.totalSeconds));
      // Only show the finished row once the user has actually finished a book —
      // otherwise it's just clutter saying "0".
      if (summary_.finishedBookCount > 0) {
        b.rowLR(tr(STR_READING_STATS_FINISHED), std::to_string(summary_.finishedBookCount));
      }
    });

  // ---- 30-day sparkline card: what the calendar replaces when the date is trusted ----
  const uint16_t today = today_;
  if (!calendar_ && today != 0 && !summary_.globalDays.empty() && fits(true, 34)) {
    layout.card(tr(STR_READING_STATS_LAST_30D), [&](CardLayout::Body& b) {
      constexpr int kSparkDays = 30;
      constexpr int kSparkHeight = 32;
      constexpr int kBarGap = 1;
      const int innerWidth = b.innerWidth();
      const int innerLeft = b.innerLeft();
      const int barWidth = std::max(2, (innerWidth - (kSparkDays - 1) * kBarGap) / kSparkDays);
      const int totalSpan = barWidth * kSparkDays + kBarGap * (kSparkDays - 1);
      const int sparkOriginX = innerLeft + (innerWidth - totalSpan) / 2;
      const int sparkOriginY = b.currentY();

      uint32_t maxSeconds = 1;
      for (int i = 0; i < kSparkDays; ++i) {
        const uint16_t d = (today > static_cast<uint16_t>(kSparkDays - 1 - i))
                               ? static_cast<uint16_t>(today - (kSparkDays - 1 - i))
                               : 0;
        const uint32_t s = ReadingStatsStore::secondsOn(summary_.globalDays, d);
        if (s > maxSeconds) maxSeconds = s;
      }

      renderer.drawLine(sparkOriginX, sparkOriginY + kSparkHeight, sparkOriginX + totalSpan,
                        sparkOriginY + kSparkHeight, true);
      for (int i = 0; i < kSparkDays; ++i) {
        const uint16_t d = (today > static_cast<uint16_t>(kSparkDays - 1 - i))
                               ? static_cast<uint16_t>(today - (kSparkDays - 1 - i))
                               : 0;
        const uint32_t s = ReadingStatsStore::secondsOn(summary_.globalDays, d);
        const int barX = sparkOriginX + i * (barWidth + kBarGap);
        const int h = s == 0 ? 1 : std::max<int>(2, (s * kSparkHeight) / maxSeconds);
        renderer.fillRect(barX, sparkOriginY + kSparkHeight - h, barWidth, h, true);
      }
      b.advance(kSparkHeight + 2);
    });
  }

  // ---- Top books card ----
  if (!topBooks_.empty() && fits(true, rowStep)) {
    layout.card(tr(STR_READING_STATS_TOP_BOOKS), [&](CardLayout::Body& b) {
      const int ellipsisWidth = renderer.getTextWidth(UI_10_FONT_ID, "…");
      constexpr int kTitleGap = 8;
      const int innerLeft = b.innerLeft();
      const int innerRight = b.innerRight();
      for (const auto& bk : topBooks_) {
        if (b.currentY() + b.rowStep() > bottom) break;
        const std::string time = formatReadingDuration(bk.totalSeconds);
        const int timeWidth = renderer.getTextWidth(UI_10_FONT_ID, time.c_str());
        renderer.drawText(UI_10_FONT_ID, innerRight - timeWidth, b.currentY(), time.c_str());

        std::string label = bk.title.empty() ? bk.docId : bk.title;
        const int maxLabelWidth = (innerRight - timeWidth - kTitleGap) - innerLeft;
        if (maxLabelWidth > 0 && renderer.getTextWidth(UI_10_FONT_ID, label.c_str()) > maxLabelWidth) {
          while (!label.empty() &&
                 renderer.getTextWidth(UI_10_FONT_ID, label.c_str()) + ellipsisWidth > maxLabelWidth) {
            utf8RemoveLastChar(label);
          }
          label += "…";
        }
        renderer.drawText(UI_10_FONT_ID, innerLeft, b.currentY(), label.c_str(), true, EpdFontFamily::BOLD);
        b.advance(b.rowStep());
      }
    });
  }

  const char* btn2 = summary_.bookCount == 0 ? "" : tr(STR_READING_STATS_BOOK_LIST);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), btn2, canShowMonth(-1) ? tr(STR_CAL_EARLIER) : "",
                                            canShowMonth(1) ? tr(STR_CAL_LATER) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
