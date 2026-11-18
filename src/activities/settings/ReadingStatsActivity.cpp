#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include "KOReaderDocumentId.h"
#include "MappedInputManager.h"
#include "ReadingSessionLog.h"
#include "ReadingStatsBookListActivity.h"
#include "RecentBooksStore.h"
#include "components/BookProgressPresentation.h"
#include "components/CardLayout.h"
#include "components/StatsDraw.h"
#include "components/UITheme.h"
#include "components/themes/PanelMetrics.h"
#include "fontIds.h"

namespace {

// Top books are picked from this many of the most-read entries, so a book held twice (a renamed copy)
// still sums into one row without reading the whole history.
constexpr size_t kTopScan = 8;
constexpr size_t kTopShown = 3;

void duration(char* out, const size_t size, const uint32_t seconds) {
  ReadingInsights::formatDuration(out, size, seconds, tr(STR_STATS_DUR_HM), tr(STR_STATS_DUR_H), tr(STR_STATS_DUR_M));
}

}  // namespace

void ReadingStatsActivity::loadSummary() {
  ReadingStatsStore::Summary summary;
  std::vector<BookReadingStats> top;
  const bool ok = READING_STATS.querySummary(summary, /*withIndex=*/true) == ReadingStatsStore::ReadResult::Ok;
  if (ok) {
    const size_t scanned = std::min(summary.byTime.size(), kTopScan);
    if (READING_STATS.queryBooksAt(summary.byTime, 0, scanned, summary.seq, top) != ReadingStatsStore::ReadResult::Ok) {
      top.clear();
    }
    for (BookReadingStats& book : top) std::vector<DayBucket>().swap(book.days);  // title and time only
    ReadingInsights::mergeDuplicates(top);
    if (top.size() > kTopShown) top.resize(kTopShown);
    std::vector<ReadingStatsStore::IndexEntry>().swap(summary.byTime);
    std::vector<DayBucket>().swap(summary.globalDays);  // nothing here goes by the day
  }

  // The book last read: the newest logged session's, else (no log yet: a card from before v3.2) the
  // book at the top of the recent list.
  ReadingInsights::SessionLog sessions;
  ReadingSessionLog::load(sessions);
  std::string docId = sessions.lastDocId;
  if (docId.empty()) {
    RecentBooksStore::Hold hold;
    const auto& recent = RECENT_BOOKS.getBooks();
    if (!recent.empty()) docId = KOReaderDocumentId::calculateFromFilename(recent.front().path);
  }
  ReadingStatsStore::BookQuery query;
  if (ok && !docId.empty() && READING_STATS.queryBook(docId, query) != ReadingStatsStore::ReadResult::Ok) {
    query.found = false;
  }
  std::vector<DayBucket>().swap(query.book.days);
  const bool lastIsCurrent = query.found && sessions.count > 0 && docId == sessions.lastDocId;

  RenderLock lock(*this);
  summary_ = std::move(summary);
  topBooks_ = std::move(top);
  sessions_ = sessions;
  current_ = query.found ? std::move(query.book) : BookReadingStats{};
  lastIsCurrent_ = lastIsCurrent;
}

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  loadSummary();
  requestUpdate();
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

  // The cards do not page: a card that would run into the button hints is left out.
  const int bottom = contentRect.y + contentRect.height - metrics.verticalSpacing;
  const int bodyH = renderer.getLineHeight(UI_10_FONT_ID);
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  const auto fits = [&](const bool titled, const int bodyHeight) {
    return layout.cursorY() + layout.cardHeight(titled, bodyHeight) <= bottom;
  };

  if (summary_.totalSeconds == 0) {
    layout.card(nullptr, [&](CardLayout::Body& b) { b.centeredMessage(tr(STR_READING_STATS_NO_DATA)); });
  }

  // ---- The current book: its ring, the time left at this pace, the last session ----
  if (!current_.docId.empty() && current_.totalSeconds > 0) {
    char line[48] = "", note[48] = "", last[64] = "", text[24];
    const ReadingInsights::TimeLeft left =
        ReadingInsights::timeLeft(current_.totalSeconds, current_.progress, ReadingInsights::averageSeconds(sessions_));
    if (current_.progress >= 100) {
      snprintf(line, sizeof(line), "%s", tr(STR_STATS_BOOK_DONE));
    } else if (left.known) {
      duration(text, sizeof(text), std::max<uint32_t>(left.seconds, 60));
      snprintf(line, sizeof(line), tr(STR_STATS_LEFT), text);
      if (left.sessions == 1) {
        snprintf(note, sizeof(note), "%s", tr(STR_STATS_ONE_SESSION_LEFT));
      } else if (left.sessions > 1) {
        snprintf(note, sizeof(note), tr(STR_STATS_SESSIONS_LEFT), static_cast<int>(left.sessions));
      }
    } else {
      snprintf(note, sizeof(note), "%s", tr(STR_STATS_ESTIMATE_LATER));
    }
    const ReadingInsights::Session* newest = ReadingInsights::newest(sessions_);
    if (lastIsCurrent_ && newest != nullptr) {
      duration(text, sizeof(text), newest->seconds);
      snprintf(last, sizeof(last), tr(STR_STATS_LAST_SESSION), text, static_cast<int>(newest->pages));
    }
    StatsDraw::Book view;
    view.percent = current_.progress;
    view.title = current_.title.empty() ? current_.docId.c_str() : current_.title.c_str();
    view.line = line;
    view.note = note;
    view.last = last;
    layout.card(nullptr, [&](CardLayout::Body& b) {
      b.advance(StatsDraw::drawBook(renderer, b.innerLeft(), b.currentY(), b.innerWidth(), view));
    });
  }

  // ---- Recent sessions: newest on the right ----
  constexpr int kBarsH = 48;
  if (summary_.totalSeconds > 0 && fits(true, kBarsH + 6 + smallH)) {
    layout.card(tr(STR_STATS_RECENT), [&](CardLayout::Body& b) {
      if (sessions_.count == 0) {
        b.centeredMessage(tr(STR_STATS_NO_SESSIONS));
        return;
      }
      uint32_t seconds[ReadingInsights::kCapacity];
      for (uint8_t i = 0; i < sessions_.count; ++i) seconds[i] = sessions_.sessions[i].seconds;
      b.advance(StatsDraw::drawSessionBars(renderer, b.innerLeft(), b.currentY(), b.innerWidth(), kBarsH, seconds,
                                           sessions_.count, ReadingInsights::kCapacity) +
                4);
      char average[24], longest[24], caption[64];
      duration(average, sizeof(average), ReadingInsights::averageSeconds(sessions_));
      duration(longest, sizeof(longest), ReadingInsights::longestSeconds(sessions_));
      snprintf(caption, sizeof(caption), tr(STR_STATS_AVG_LONGEST), average, longest);
      renderer.drawText(SMALL_FONT_ID, b.innerLeft(), b.currentY(),
                        renderer.truncatedText(SMALL_FONT_ID, caption, b.innerWidth()).c_str());
      b.advance(smallH);
    });
  }

  // ---- Milestones: hours, pages, books finished, each with the next one and how far to go ----
  const int milestoneH = bodyH + 2 + 6 + 2 + smallH;
  if (summary_.totalSeconds > 0 && fits(true, 3 * milestoneH + 2 * 8)) {
    layout.card(tr(STR_STATS_MILESTONES), [&](CardLayout::Body& b) {
      using ReadingInsights::Kind;
      const struct {
        Kind kind;
        uint32_t amount;
        const char* label;
        const char* goal;
      } rows[] = {{Kind::Hours, summary_.totalSeconds, tr(STR_STATS_HOURS_READ), tr(STR_STATS_GOAL_HOURS)},
                  {Kind::Pages, summary_.totalPagesTurned, tr(STR_READING_STATS_PAGES), tr(STR_STATS_GOAL_PAGES)},
                  {Kind::Books, summary_.finishedBookCount, tr(STR_STATS_BOOKS_FINISHED), tr(STR_STATS_GOAL_BOOKS)}};
      for (size_t i = 0; i < 3; ++i) {
        const ReadingInsights::Milestone m = ReadingInsights::milestone(rows[i].kind, rows[i].amount);
        char value[16], threshold[16], goal[32], toGo[24], next[72];
        ReadingInsights::groupThousands(value, sizeof(value), m.value);
        ReadingInsights::groupThousands(threshold, sizeof(threshold), m.next);
        snprintf(goal, sizeof(goal), rows[i].goal, threshold);
        if (m.kind == Kind::Hours) {
          duration(toGo, sizeof(toGo), std::max<uint32_t>(m.toGo, 60));
        } else {
          ReadingInsights::groupThousands(toGo, sizeof(toGo), m.toGo);
        }
        snprintf(next, sizeof(next), tr(STR_STATS_NEXT), goal, toGo);
        StatsDraw::Milestone view;
        view.label = rows[i].label;
        view.value = value;
        view.next = next;
        view.permille = m.permille;
        if (i > 0) b.advance(8);
        b.advance(StatsDraw::drawMilestone(renderer, b.innerLeft(), b.currentY(), b.innerWidth(), view));
      }
    });
  }

  // ---- Top books ----
  if (!topBooks_.empty() && fits(true, bodyH + 2)) {
    layout.card(tr(STR_READING_STATS_TOP_BOOKS), [&](CardLayout::Body& b) {
      constexpr int kTitleGap = 8;
      for (const auto& bk : topBooks_) {
        if (b.currentY() + b.rowStep() > bottom) break;
        const std::string time = BookProgressPresentation::formatReadingDuration(bk.totalSeconds);
        const int timeWidth = renderer.getTextWidth(UI_10_FONT_ID, time.c_str());
        renderer.drawText(UI_10_FONT_ID, b.innerRight() - timeWidth, b.currentY(), time.c_str());
        const std::string label =
            renderer.truncatedText(UI_10_FONT_ID, bk.title.empty() ? bk.docId.c_str() : bk.title.c_str(),
                                   b.innerWidth() - timeWidth - kTitleGap, EpdFontFamily::BOLD);
        renderer.drawText(UI_10_FONT_ID, b.innerLeft(), b.currentY(), label.c_str(), true, EpdFontFamily::BOLD);
        b.advance(b.rowStep());
      }
    });
  }

  const char* btn2 = summary_.bookCount == 0 ? "" : tr(STR_READING_STATS_BOOK_LIST);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), btn2, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
