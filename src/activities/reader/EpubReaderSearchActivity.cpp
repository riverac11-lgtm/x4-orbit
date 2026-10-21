#include "EpubReaderSearchActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <utility>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

EpubReaderSearchActivity::EpubReaderSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   const std::shared_ptr<Epub>& epub, const int startSpineIndex,
                                                   std::string* queryStore)
    : UiListActivity("EpubReaderSearch", renderer, mappedInput),
      epub(epub),
      startSpineIndex(startSpineIndex),
      queryStore(queryStore),
      query(queryStore ? *queryStore : std::string()) {}

void EpubReaderSearchActivity::onEnter() {
  UiListActivity::onEnter();
  // Same trade as the chapter list: let the reader's glyph arenas go so this
  // list can keep its own rows resident.
  if (auto* fcm = renderer.getFontCacheManager()) fcm->clearCache();
  updateHeader();
  askForQuery();
}

void EpubReaderSearchActivity::askForQuery() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SEARCH_BOOK), query, 64);
  if (!keyboard) {
    LOG_ERR("SRCH", "OOM: keyboard");
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      ActivityResult cancelled;
      cancelled.isCancelled = true;
      setResult(std::move(cancelled));
      finish();
      return;
    }
    startSearch(std::get<KeyboardResult>(result.data).text);
  });
}

void EpubReaderSearchActivity::startSearch(const std::string& text) {
  RenderLock lock;
  query = text;
  if (queryStore) *queryStore = text;
  needle = booksearch::foldQuery(text);
  entries.clear();
  chapterTitles.clear();
  lastTitledSpine = -1;
  rowItems.clear();
  nav.selected = 0;
  nav.top = 0;
  capped = false;
  spinesDone = 0;
  spineCount = epub ? epub->getSpineItemsCount() : 0;
  if (startSpineIndex < 0 || startSpineIndex >= spineCount) startSpineIndex = 0;
  searching = !needle.empty() && spineCount > 0;
  lastRepaintMs = 0;  // first progress paint right away
  updateHeader();
  requestUpdate();
}

void EpubReaderSearchActivity::loop() {
  if (searching) searchNextChapter();
  UiListActivity::loop();
}

void EpubReaderSearchActivity::searchNextChapter() {
  HalPowerManager::Lock powerLock;  // inflate + parse at full clock
  const int spine = (startSpineIndex + spinesDone) % spineCount;
  const auto href = epub->getSpineItem(spine).href;

  booksearch::ChapterSearcher searcher(needle, MAX_HITS - entries.size());
  // Any button gives way at once: the press is handled this pass, and the
  // chapter is searched again from its start on the next one.
  searcher.setAbortProbe([] { return gpio.anyButtonDownNow(); });
  const unsigned long t0 = millis();
  const bool read = !href.empty() && searcher.ok() && epub->readItemContentsToStream(href, searcher, 1024, true);
  searcher.finish();
  if (searcher.aborted()) return;
  if (!read) LOG_ERR("SRCH", "Could not search spine %d", spine);
  LOG_DBG("SRCH", "Spine %d: %u hits in %lums", spine, static_cast<unsigned>(searcher.hits().size()), millis() - t0);

  {
    RenderLock lock;
    for (auto& hit : searcher.takeHits()) {
      if (spine != lastTitledSpine) {
        const int toc = epub->getTocIndexForSpineIndex(spine);
        std::string title = toc >= 0 ? epub->getTocItem(toc).title : std::string();
        if (title.empty()) {
          char fallback[32];
          snprintf(fallback, sizeof(fallback), "%s %d", tr(STR_CHAPTER), spine + 1);
          title = fallback;
        }
        chapterTitles.push_back(std::move(title));
        lastTitledSpine = spine;
      }
      entries.push_back(Entry{static_cast<uint16_t>(spine), static_cast<uint16_t>(chapterTitles.size() - 1),
                              hit.visibleOffset, hit.visibleEnd, std::move(hit.snippet)});
    }
    spinesDone++;
    if (entries.size() >= MAX_HITS) {
      capped = true;
      searching = false;
    } else if (spinesDone >= spineCount) {
      searching = false;
    }
    rebuildRows();
    updateHeader();
  }

  const unsigned long now = millis();
  if (!searching || now - lastRepaintMs >= REPAINT_INTERVAL_MS) {
    lastRepaintMs = now;
    requestUpdate();
  }
}

void EpubReaderSearchActivity::stopSearching() {
  RenderLock lock;
  searching = false;
  updateHeader();
  requestUpdate();
}

void EpubReaderSearchActivity::rebuildRows() {
  rowItems.clear();
  rowItems.reserve(entries.size());
  for (const auto& e : entries) {
    fui::ListItem item;
    item.label = chapterTitles[e.titleIndex].c_str();
    item.subtitle = e.snippet.c_str();
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
}

void EpubReaderSearchActivity::updateHeader() {
  char status[64];
  if (searching) {
    snprintf(status, sizeof(status), tr(STR_SEARCH_PROGRESS), spinesDone, spineCount,
             static_cast<unsigned>(entries.size()));
  } else if (capped) {
    snprintf(status, sizeof(status), tr(STR_SEARCH_FIRST_RESULTS), static_cast<unsigned>(entries.size()));
  } else {
    snprintf(status, sizeof(status), tr(STR_SEARCH_RESULTS), static_cast<unsigned>(entries.size()));
  }
  header = query.empty() ? std::string(tr(STR_SEARCH_BOOK)) : "\xE2\x80\x9C" + query + "\xE2\x80\x9D  " + status;
}

void EpubReaderSearchActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    if (searching) {
      screen.centeredText(tr(STR_SEARCH_SEARCHING), screen.theme().bodyText);
    } else if (!needle.empty()) {
      screen.centeredText(tr(STR_SEARCH_NO_MATCHES), screen.theme().bodyText);
    }
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void EpubReaderSearchActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(entries.size())) return;
  app.clearTapFlash();
  const Entry& e = entries[index];
  setResult(SearchResult{e.spineIndex, e.visibleOffset, e.visibleEnd});
  finish();
}

void EpubReaderSearchActivity::onBackButton() {
  if (searching) {
    stopSearching();  // keep what was found; Back again leaves
    return;
  }
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
