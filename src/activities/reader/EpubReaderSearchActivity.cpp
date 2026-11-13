#include "EpubReaderSearchActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <utility>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

EpubReaderSearchActivity::EpubReaderSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   std::shared_ptr<Epub> epub, const int startSpineIndex,
                                                   std::string* queryStore, bool* wholeWordStore)
    : Activity("EpubReaderSearch", renderer, mappedInput),
      epub(std::move(epub)),
      startSpineIndex(startSpineIndex),
      queryStore(queryStore),
      wholeWordStore(wholeWordStore),
      query(queryStore ? *queryStore : std::string()) {}

void EpubReaderSearchActivity::onEnter() {
  Activity::onEnter();
  std::string text;
  if (Storage.exists(searchlist::RECENT_PATH) &&
      Storage.readFileToString("SRCH", searchlist::RECENT_PATH, 1024, text)) {
    recent = searchlist::parseRecent(text);
  }
  if (loadKept()) {
    requestUpdate();
    return;
  }
  newSearch();
}

// The list the last visit kept. False -- and nothing shown -- when there is none or it does
// not read back whole.
bool EpubReaderSearchActivity::loadKept() {
  if (!epub) return false;
  HalFile file;
  const std::string path = searchlist::pathFor(epub->getCachePath());
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("SRCH", path, file)) return false;
  searchlist::Header header;
  std::vector<searchlist::Pos> hits;
  bool ok = searchlist::readPositions(file, header, hits) && !hits.empty();
  RenderLock lock(*this);
  entries.reserve(hits.size());
  for (size_t i = 0; ok && i < hits.size(); ++i) {
    std::string title;
    std::string snippet;
    ok = searchlist::readText(file, title, snippet) && !(title.empty() && chapterTitles.empty());
    if (!ok) break;
    if (!title.empty()) chapterTitles.push_back(std::move(title));
    entries.push_back(Entry{hits[i].spine, static_cast<uint16_t>(chapterTitles.size() - 1), hits[i].offset, hits[i].end,
                            std::move(snippet)});
  }
  file.close();
  if (!ok) {
    entries.clear();
    chapterTitles.clear();
    return false;
  }
  query = header.query;
  wholeWord = (header.flags & searchlist::FLAG_WHOLE_WORD) != 0;
  capped = (header.flags & searchlist::FLAG_CAPPED) != 0;
  spinesSkipped = header.skipped;
  openedIndex = header.current;
  selectorIndex = header.current > 0 ? header.current : 0;
  haveList = true;
  return true;
}

// Writes the list to the book's cache folder, `current` being the hit opened (-1: none). An
// unchanged list only has that index rewritten. A failed write leaves no file: the reader then
// simply has no list to step through.
void EpubReaderSearchActivity::keep(const int16_t current) {
  if (!epub || !haveList) return;
  const std::string path = searchlist::pathFor(epub->getCachePath());
  HalFile file;
  if (!dirty) {
    if (current != openedIndex && Storage.openFileForUpdate("SRCH", path, file)) {
      searchlist::writeCurrent(file, current);
      file.close();
    }
    return;
  }
  if (entries.empty()) {
    Storage.remove(path.c_str());
    return;
  }
  if (!Storage.openFileForWrite("SRCH", path, file)) return;
  searchlist::Header header;
  header.current = current;
  header.count = static_cast<uint16_t>(entries.size());
  header.flags =
      static_cast<uint8_t>((wholeWord ? searchlist::FLAG_WHOLE_WORD : 0) | (capped ? searchlist::FLAG_CAPPED : 0));
  header.skipped = static_cast<uint16_t>(spinesSkipped);
  header.query = query;
  bool ok = searchlist::writeHeader(file, header);
  for (size_t i = 0; ok && i < entries.size(); ++i) {
    ok = searchlist::writePos(file, {entries[i].spineIndex, entries[i].visibleOffset, entries[i].visibleEnd});
  }
  static const std::string sameChapter;
  for (size_t i = 0; ok && i < entries.size(); ++i) {
    const bool newChapter = i == 0 || entries[i].titleIndex != entries[i - 1].titleIndex;
    ok = searchlist::writeText(file, newChapter ? chapterTitles[entries[i].titleIndex] : sameChapter,
                               entries[i].snippet);
  }
  file.close();
  if (!ok) {
    LOG_ERR("SRCH", "Could not keep the result list");
    Storage.remove(path.c_str());
  }
}

// Back out of the screen: the list is kept for the next visit.
void EpubReaderSearchActivity::leave() {
  keep(openedIndex);
  cancelAndFinish();
}

// Recent queries first when there are any; their first row opens the keyboard.
void EpubReaderSearchActivity::newSearch() {
  if (recent.empty()) {
    askForQuery();
    return;
  }
  RenderLock lock(*this);
  searching = false;
  view = View::Recent;
  recentIndex = 0;
  requestUpdate();
}

void EpubReaderSearchActivity::cancelAndFinish() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void EpubReaderSearchActivity::askForQuery() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SEARCH_BOOK), query, 64, InputType::Text),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          // Back to the screen the keyboard was opened from, when there is one.
          if (view == View::Recent || haveList) {
            requestUpdate();
            return;
          }
          cancelAndFinish();
          return;
        }
        startSearch(std::get<KeyboardResult>(result.data).text);
      });
}

void EpubReaderSearchActivity::startSearch(const std::string& text) {
  RenderLock lock(*this);
  query = text;
  if (queryStore) *queryStore = text;
  if (wholeWordStore) *wholeWordStore = wholeWord;
  needle = booksearch::foldQuery(text);
  if (!needle.empty() && searchlist::addRecent(recent, text)) {
    Storage.writeFile(searchlist::RECENT_PATH, String(searchlist::joinRecent(recent).c_str()));
  }
  view = View::Results;
  haveList = true;
  dirty = true;
  openedIndex = -1;
  entries.clear();
  chapterTitles.clear();
  lastTitledSpine = -1;
  selectorIndex = 0;
  capped = false;
  spinesDone = 0;
  spinesSkipped = 0;
  spineCount = epub ? epub->getSpineItemsCount() : 0;
  if (startSpineIndex < 0 || startSpineIndex >= spineCount) startSpineIndex = 0;
  searching = !needle.empty() && spineCount > 0;
  lastRepaintMs = millis();
  requestUpdate();
}

void EpubReaderSearchActivity::searchNextChapter() {
  HalPowerManager::Lock powerLock;  // inflate + parse at full clock
  const int spine = (startSpineIndex + spinesDone) % spineCount;
  const auto href = epub->getSpineItem(spine).href;

  booksearch::ChapterSearcher searcher(needle, MAX_HITS - entries.size(), 28, {.wholeWord = wholeWord});
  // A press gives way at once: the loop handles it this pass, and the chapter is searched
  // again from its start on the next one.
  searcher.setAbortProbe([] { return gpio.hasPendingInput(); });
  const unsigned long t0 = millis();
  const bool read =
      !href.empty() && searcher.ok() && epub->readItemContentsToStream(href, searcher, 1024, searcher.stopFlag());
  searcher.finish();
  if (searcher.aborted()) return;
  // A capped search stops the stream on purpose; anything else that failed is reported.
  const bool failed = !read && !searcher.full();
  if (failed) LOG_ERR("SRCH", "Could not search spine %d", spine);
  LOG_DBG("SRCH", "Spine %d: %u hits in %lums", spine, static_cast<unsigned>(searcher.hits().size()), millis() - t0);

  {
    RenderLock lock(*this);
    for (auto& hit : searcher.takeHits()) {
      if (spine != lastTitledSpine) {
        const int toc = epub->getTocIndexForSpineIndex(spine);
        std::string title = toc >= 0 ? epub->getTocItem(toc).title : std::string();
        if (title.empty()) title = std::string(tr(STR_SECTION_PREFIX)) + std::to_string(spine + 1);
        chapterTitles.push_back(std::move(title));
        lastTitledSpine = spine;
      }
      entries.push_back(Entry{static_cast<uint16_t>(spine), static_cast<uint16_t>(chapterTitles.size() - 1),
                              hit.visibleOffset, hit.visibleEnd, std::move(hit.snippet)});
    }
    if (failed) spinesSkipped++;
    spinesDone++;
    if (entries.size() >= MAX_HITS) {
      capped = true;
      searching = false;
    } else if (spinesDone >= spineCount) {
      searching = false;
    }
  }

  const unsigned long now = millis();
  if (!searching || now - lastRepaintMs >= REPAINT_INTERVAL_MS) {
    lastRepaintMs = now;
    requestUpdate();
  }
}

void EpubReaderSearchActivity::loop() {
  ButtonEventManager::ButtonEvent ev;
  while (buttonEvents.consumeEvent(ev)) {
    if (ev.type != ButtonEventManager::PressType::Short) continue;
    if (ev.button == MappedInputManager::Button::Back) {
      if (searching) {
        RenderLock lock(*this);
        searching = false;  // keep what was found; Back again leaves
        requestUpdate();
        return;
      }
      if (view == View::Recent && haveList) {
        RenderLock lock(*this);
        view = View::Results;  // back to the list the new search was started from
        requestUpdate();
        return;
      }
      leave();
      return;
    }
    if (view == View::Recent) {
      if (ev.button == MappedInputManager::Button::Confirm) {
        if (recentIndex == 0) {
          askForQuery();
        } else {
          const std::string text = recent[recentIndex - 1];  // a copy: the search reorders `recent`
          startSearch(text);
        }
        return;
      }
      continue;
    }
    if (ev.button == MappedInputManager::Button::Confirm && !entries.empty()) {
      const Entry& e = entries[selectorIndex];
      if (queryStore) *queryStore = query;
      if (wholeWordStore) *wholeWordStore = wholeWord;
      keep(static_cast<int16_t>(selectorIndex));
      setResult(SearchResult{e.spineIndex, e.visibleOffset, e.visibleEnd});
      finish();
      return;
    }
    // Left and Right carry the two actions shown in the hints, so the list moves by Up and
    // Down alone here (held down to run through it).
    if (ev.button == MappedInputManager::buttonFor(MappedInputManager::Direction::Left) && haveList) {
      wholeWord = !wholeWord;
      const std::string text = query;
      startSearch(text);
      return;
    }
    if (ev.button == MappedInputManager::buttonFor(MappedInputManager::Direction::Right)) {
      newSearch();
      return;
    }
  }

  const bool onRecent = view == View::Recent;
  const int total = onRecent ? static_cast<int>(recent.size()) + 1 : static_cast<int>(entries.size());
  int& index = onRecent ? recentIndex : selectorIndex;
  if (total > 0) {
    buttonNavigator.onNextList(ButtonNavigator::getStepNextButtons(), index, total, [this] { requestUpdate(); });
    buttonNavigator.onPreviousList(ButtonNavigator::getStepPreviousButtons(), index, total,
                                   [this] { requestUpdate(); });
  }

  if (searching) searchNextChapter();
}

std::string EpubReaderSearchActivity::headerText() const {
  char status[64];
  if (searching) {
    snprintf(status, sizeof(status), tr(STR_SEARCH_PROGRESS), spinesDone, spineCount,
             static_cast<unsigned>(entries.size()));
  } else if (capped) {
    snprintf(status, sizeof(status), tr(STR_SEARCH_FIRST_RESULTS), static_cast<unsigned>(entries.size()));
  } else {
    snprintf(status, sizeof(status), tr(STR_SEARCH_RESULTS), static_cast<unsigned>(entries.size()));
  }
  return "\xE2\x80\x9C" + query + "\xE2\x80\x9D  " + status;
}

void EpubReaderSearchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect contentRect = UITheme::getContentRect(renderer, true, true);
  int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  if (view == View::Recent) {
    GUI.drawHeader(renderer, UITheme::getHeaderRect(renderer), tr(STR_SEARCH_BOOK));
    GUI.drawList(
        renderer,
        Rect{contentRect.x, contentTop, contentRect.width, contentRect.height - contentTop - metrics.verticalSpacing},
        static_cast<int>(recent.size()) + 1, recentIndex,
        [this](const int i) { return i == 0 ? std::string(tr(STR_SEARCH_TYPE_NEW)) : recent[i - 1]; });
    const auto recentHints =
        mappedInput.mapHints(tr(STR_BACK), tr(STR_SELECT), "", "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, recentHints.front.btn1, recentHints.front.btn2, recentHints.front.btn3,
                        recentHints.front.btn4);
    GUI.drawSideButtonHints(renderer, recentHints.side.up, recentHints.side.down);
    renderer.displayBuffer();
    return;
  }
  GUI.drawHeader(renderer, UITheme::getHeaderRect(renderer), headerText().c_str());
  if (wholeWord) {
    renderer.drawText(UI_10_FONT_ID, contentRect.x + metrics.contentSidePadding, contentTop, tr(STR_SEARCH_WHOLE_ONLY));
    contentTop += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  }
  if (spinesSkipped > 0 && !searching) {
    char note[64];
    snprintf(note, sizeof(note), tr(STR_SEARCH_SKIPPED), spinesSkipped);
    renderer.drawText(UI_10_FONT_ID, contentRect.x + metrics.contentSidePadding, contentTop, note);
    contentTop += renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  }
  const int contentHeight = contentRect.height - contentTop - metrics.verticalSpacing;

  if (entries.empty()) {
    renderer.drawText(UI_10_FONT_ID, contentRect.x + metrics.contentSidePadding, contentTop + 20,
                      searching ? tr(STR_SEARCHING) : tr(STR_SEARCH_NO_MATCHES));
  } else {
    GUI.drawList(
        renderer, Rect{contentRect.x, contentTop, contentRect.width, contentHeight}, static_cast<int>(entries.size()),
        selectorIndex, [this](const int i) { return chapterTitles[entries[i].titleIndex]; },
        [this](const int i) { return entries[i].snippet; });
  }

  const bool hasItems = !entries.empty();
  const auto hints = mappedInput.mapHints(tr(STR_BACK), hasItems ? tr(STR_OPEN) : "",
                                          wholeWord ? tr(STR_SEARCH_ANY_MATCH) : tr(STR_SEARCH_WHOLE_WORD),
                                          tr(STR_SEARCH_NEW), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.front.btn1, hints.front.btn2, hints.front.btn3, hints.front.btn4);
  GUI.drawSideButtonHints(renderer, hints.side.up, hints.side.down);
  renderer.displayBuffer();
}

ListRowTap::Result EpubReaderSearchActivity::selectListRow(const int index) {
  if (view == View::Recent) return ListRowTap::apply(index, static_cast<int>(recent.size()) + 1, recentIndex);
  return ListRowTap::apply(index, static_cast<int>(entries.size()), selectorIndex);
}

bool EpubReaderSearchActivity::pageList(ListPageDirection /*direction*/) { return true; }
