#include "OrbitTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "BatteryLog.h"
#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "RecentBooksStore.h"
#include "components/BookProgressPresentation.h"
#include "components/StatsDraw.h"
#include "components/UITheme.h"
#include "components/themes/TapTargets.h"
#include "components/themes/orbit/OrbitHomeDraw.h"
#include "fontIds.h"
#include "util/ReadingInsights.h"

namespace {

using OrbitHomeLayout::Box;
using OrbitHomeLayout::Variant;
using View = OrbitHomeDraw::View;

// A recent book's title, or its file name when the book never gave one (a .txt).
std::string displayTitle(const RecentBook& book) {
  if (!book.title.empty()) return book.title;
  const size_t slash = book.path.find_last_of('/');
  std::string name = book.path.substr(slash == std::string::npos ? 0 : slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0) name.erase(dot);
  return name;
}

// The time of day only when the clock knows it: a never-synced or stale clock would greet the
// reader with "Good evening" at breakfast.
const char* greeting() {
  if (!HalClock::isSynced() || HalClock::isStaleRestore()) return tr(STR_ORBIT_HELLO);
  const time_t now = time(nullptr);
  struct tm local{};
  localtime_r(&now, &local);
  if (local.tm_hour >= 4 && local.tm_hour < 12) return tr(STR_ORBIT_GOOD_MORNING);
  if (local.tm_hour >= 12 && local.tm_hour < 18) return tr(STR_ORBIT_GOOD_AFTERNOON);
  return tr(STR_ORBIT_GOOD_EVENING);
}

struct CoverContext {
  const std::vector<RecentBook>* books;
  int height;
};

// One cover slot: the cached thumbnail the cover pass made for this variant's height, centred in
// the slot, or -- while it is still being made, or for a book without a cover -- a plain tile with
// the title. Streams the BMP from the card row by row (GfxRenderer::drawBitmap); nothing is kept.
void paintCover(void* context, GfxRenderer& renderer, const int index, const Box& slot) {
  const auto& ctx = *static_cast<const CoverContext*>(context);
  if (index < 0 || index >= static_cast<int>(ctx.books->size())) return;
  const RecentBook& book = (*ctx.books)[index];
  renderer.fillRect(slot.x, slot.y, slot.w, slot.h, false);
  bool drawn = false;
  if (!book.coverBmpPath.empty()) {
    FsFile file;
    if (Storage.openFileForRead("HOME", UITheme::getCoverThumbPath(book.coverBmpPath, ctx.height), file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok &&
          !UITheme::isCoverPlaceholderBmp(bitmap.getWidth(), bitmap.getHeight())) {
        const int w = std::min(bitmap.getWidth(), slot.w);
        const int h = std::min(bitmap.getHeight(), slot.h);
        renderer.drawBitmap(bitmap, slot.x + (slot.w - w) / 2, slot.y + (slot.h - h) / 2, slot.w, slot.h);
        drawn = true;
      }
      file.close();
    }
  }
  if (!drawn) {
    // The title on a quiet tile: a book is recognisable by its name where it has no picture.
    renderer.fillRectDither(slot.x, slot.y, slot.w, slot.h / 5, Color::LightGray);
    const int font = slot.w >= 150 ? UI_12_FONT_ID : UI_10_FONT_ID;
    const int lineH = renderer.getLineHeight(font);
    const std::string title = displayTitle(book);
    const auto lines =
        renderer.wrappedText(font, title.c_str(), slot.w - 12, std::max(1, (slot.h / 2) / lineH), EpdFontFamily::BOLD);
    int y = slot.y + slot.h / 4 + 8;
    for (const auto& line : lines) {
      const int w = renderer.getTextWidth(font, line.c_str(), EpdFontFamily::BOLD);
      renderer.drawText(font, slot.x + (slot.w - w) / 2, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += lineH;
    }
  }
  renderer.drawRect(slot.x, slot.y, slot.w, slot.h, true);
}

}  // namespace

void OrbitTheme::drawOrbitHome(GfxRenderer& renderer, const int top, const int bottom,
                               const std::vector<RecentBook>& books, const bool hasCurrent, const bool shelfIsAdded,
                               const int selector, const int menuCount,
                               const std::function<std::string(int)>& menuLabel,
                               const std::function<UIIcon(int)>& menuIcon) const {
  OrbitHomeDraw::View view;
  view.variant = variant_;
  view.width = renderer.getScreenWidth();
  view.top = top;
  view.bottom = bottom;
  view.hasBook = hasCurrent && !books.empty();
  if (view.hasBook) {
    const RecentBook& book = books.front();
    view.book.title = displayTitle(book);
    view.book.author = book.author;
    view.book.series = book.series;
    view.book.percent = getRecentBookProgressPercent(book);
    view.book.eta = BookProgressPresentation::etaText(book, view.book.percent);
    view.book.history = BookProgressPresentation::historyLine(book);
  }
  const int bookCount = static_cast<int>(books.size());
  view.shelfCount = variant_ == Variant::C ? std::max(0, bookCount - (view.hasBook ? 1 : 0)) : 0;

  view.menuCount = std::min(menuCount, OrbitHomeLayout::kMaxMenu);
  std::string labels[OrbitHomeLayout::kMaxMenu];
  for (int i = 0; i < view.menuCount; ++i) {
    labels[i] = menuLabel(i);
    view.menuLabel[i] = labels[i].c_str();
    view.menuIcon[i] = menuIcon(i);
    view.menuBitmap[i] = iconForName(view.menuIcon[i], 32);
    if (view.menuBitmap[i] == nullptr) view.menuBitmap[i] = iconForName(view.menuIcon[i], 24);
  }
  // The Reading Stats graphic, from the stats Home prefetched on entry (no file read here): the current
  // book's ring and the milestone closest to done.
  char statsGoal[32], statsToGo[32], statsCompact[48];
  {
    using ReadingInsights::Kind;
    View::Stats& st = view.stats;
    st.hasHistory = READING_STATS.recentHasHistory();
    st.percent = view.hasBook ? view.book.percent : -1;
    const ReadingInsights::Milestone milestones[] = {
        ReadingInsights::milestone(Kind::Hours, READING_STATS.recentTotalSeconds()),
        ReadingInsights::milestone(Kind::Pages, READING_STATS.recentTotalPages()),
        ReadingInsights::milestone(Kind::Books, READING_STATS.recentFinishedBooks())};
    const ReadingInsights::Milestone& m = milestones[ReadingInsights::closest(milestones, 3)];
    char number[16], amount[24];
    ReadingInsights::groupThousands(number, sizeof(number), m.next);
    snprintf(statsGoal, sizeof(statsGoal),
             m.kind == Kind::Hours   ? tr(STR_STATS_GOAL_HOURS)
             : m.kind == Kind::Pages ? tr(STR_STATS_GOAL_PAGES)
                                     : tr(STR_STATS_GOAL_BOOKS),
             number);
    if (m.kind == Kind::Hours) {
      // Whole hours past the first, so the line stays short enough for a half-width tile.
      const uint32_t toGo = m.toGo >= 3600 ? (m.toGo + 3599) / 3600 * 3600 : std::max<uint32_t>(m.toGo, 60);
      ReadingInsights::formatDuration(amount, sizeof(amount), toGo, tr(STR_STATS_DUR_HM), tr(STR_STATS_DUR_H),
                                      tr(STR_STATS_DUR_M));
    } else {
      ReadingInsights::groupThousands(amount, sizeof(amount), m.toGo);
    }
    snprintf(statsToGo, sizeof(statsToGo), tr(STR_STATS_TO_GO), amount);
    snprintf(statsCompact, sizeof(statsCompact), tr(STR_STATS_NEXT_SHORT), statsGoal);
    st.heading = tr(STR_STATS_NEXT_MILESTONE);
    st.goal = statsGoal;
    st.toGo = statsToGo;
    st.compact = statsCompact;
    st.empty = tr(STR_ORBIT_STATS_EMPTY);
  }
  view.selected = selector;
  view.caption = variant_ == Variant::C ? greeting() : tr(STR_ORBIT_CONTINUE);
  view.shelfLabel = shelfIsAdded ? tr(STR_ORBIT_RECENTLY_ADDED) : tr(STR_ORBIT_RECENTLY_READ);
  view.noBookTitle = tr(STR_NO_OPEN_BOOK);
  view.noBookHint = tr(STR_ORBIT_START_HINT);
  view.leftFormat = tr(STR_ORBIT_TIME_LEFT);
  CoverContext covers{&books, OrbitHomeLayout::coverHeight(variant_)};
  view.drawCover = paintCover;
  view.coverCtx = &covers;

  const OrbitHomeLayout::Frame frame = OrbitHomeDraw::layout(renderer, view);

  // Touch: the book block and shelf covers carry their book index, the menu its entry index --
  // the values HomeActivity::handleHomeTouch() expects from every home theme.
  {
    TapTargets::Recorder::Builder coverTargets;
    if (view.hasBook) coverTargets.add(frame.book.x, frame.book.y, frame.book.w, frame.book.h, 0);
    const int first = view.hasBook ? 1 : 0;
    for (int i = 0; i < frame.shelfCount; ++i) {
      const Box& s = frame.shelf[i];
      coverTargets.add(s.x, s.y, s.w, s.h, first + i);
    }
    TapTargets::homeCovers().record(coverTargets);
    TapTargets::Recorder::Builder menuTargets;
    for (int i = 0; i < frame.menuCount; ++i) {
      const Box& m = frame.menu[i];
      menuTargets.add(m.x, m.y, m.w, m.h, i);
    }
    TapTargets::homeMenu().record(menuTargets);
  }

  OrbitHomeDraw::draw(renderer, view, frame);
}

void OrbitTheme::drawBatteryDays(const GfxRenderer& renderer, const Rect header) const {
  const uint16_t percent = powerManager.getBatteryPercentage();
  char text[24];
  if (!BatteryLog::format(text, sizeof(text), static_cast<uint8_t>(percent), gpio.isUsbConnected())) return;
  // LyraTheme::drawHeader's battery: the icon at the right, its percentage left of it unless hidden.
  int right = header.x + header.width - 12 - LyraMetrics::values.batteryWidth;
  if (SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS) {
    const std::string pct = std::to_string(percent) + "%";
    right -= BaseTheme::batteryPercentSpacing + renderer.getTextWidth(SMALL_FONT_ID, pct.c_str());
  }
  StatsDraw::drawQuietText(renderer, right - 10, header.y + 5, text);
}
