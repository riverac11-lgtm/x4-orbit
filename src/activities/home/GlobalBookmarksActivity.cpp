#include "GlobalBookmarksActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "../ActivityManager.h"
#include "BookmarkStore.h"
#include "CrossPointState.h"
#include "FileBrowserActivity.h"
#include "GlobalBookmarkIndex.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

void GlobalBookmarksActivity::onEnter() {
  indexHold.emplace();
  GLOBAL_BOOKMARKS.reconcile();
  rebuildRows();

  int selected = firstSelectableIndex();
  if (restoreHint.target == ReturnTo::GlobalBookmarks) {
    const auto& entries = GLOBAL_BOOKMARKS.getEntries();
    if (!restoreHint.selectionContext.empty() && restoreHint.selectBookmarkIndex >= 0) {
      for (size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (row.isSeparator) continue;
        if (row.bookmarkIndex == static_cast<size_t>(restoreHint.selectBookmarkIndex) &&
            row.bookIndex < entries.size() && entries[row.bookIndex].sourcePath == restoreHint.selectionContext) {
          selected = static_cast<int>(i);
          break;
        }
      }
    }
    restoreHint = {};
  }

  RenderLock lock(*this);
  UiListActivity::onEnter();
  app.on(ACTION_USER, &GlobalBookmarksActivity::tabActionTrampoline, this);
  // Opened from the tab bar (Left/Right there), or with nothing to select: the focus is on the bar.
  nav.reset(tabBarOnEnter || selected < 0 ? -1 : selected);
  buttonNavigator.resync(ButtonNavigator::getStepNextButtons(), ButtonNavigator::getStepPreviousButtons());
}

void GlobalBookmarksActivity::onExit() {
  UiListActivity::onExit();
  rows.clear();
  indexHold.reset();
  // Home's Library entry reopens the tab left last (FileBrowserActivity::onExit for the others).
  if (APP_STATE.libraryTab != static_cast<uint8_t>(LibraryTab::Bookmarks)) {
    APP_STATE.libraryTab = static_cast<uint8_t>(LibraryTab::Bookmarks);
    APP_STATE.saveToFile();
  }
}

void GlobalBookmarksActivity::rebuildRows() {
  rows.clear();
  const auto& entries = GLOBAL_BOOKMARKS.getEntries();
  for (size_t bi = 0; bi < entries.size(); bi++) {
    const auto& entry = entries[bi];
    if (entry.bookmarks.empty()) continue;
    Row sep;
    sep.isSeparator = true;
    sep.bookIndex = bi;
    rows.push_back(sep);
    for (size_t mi = 0; mi < entry.bookmarks.size(); mi++) {
      Row row;
      row.isSeparator = false;
      row.bookIndex = bi;
      row.bookmarkIndex = mi;
      rows.push_back(row);
    }
  }
}

bool GlobalBookmarksActivity::isSeparatorRow(int index) const {
  return index >= 0 && index < static_cast<int>(rows.size()) && rows[index].isSeparator;
}

int GlobalBookmarksActivity::firstSelectableIndex() const {
  for (size_t i = 0; i < rows.size(); i++) {
    if (!rows[i].isSeparator) return static_cast<int>(i);
  }
  return -1;
}

std::string GlobalBookmarksActivity::getRowTitle(int index) const {
  if (index < 0 || index >= static_cast<int>(rows.size())) return {};
  const auto& row = rows[index];
  const auto& entries = GLOBAL_BOOKMARKS.getEntries();
  if (row.bookIndex >= entries.size()) return {};
  const auto& entry = entries[row.bookIndex];

  if (row.isSeparator) return entry.title.empty() ? entry.sourcePath : entry.title;

  if (row.bookmarkIndex >= entry.bookmarks.size()) return {};
  const auto& bm = entry.bookmarks[row.bookmarkIndex];
  if (!bm.name.empty()) return bm.name;

  char buf[64];
  if (entry.isTxt) {
    snprintf(buf, sizeof(buf), "%s%d", tr(STR_PAGE_PREFIX), bm.pageNumber + 1);
  } else {
    snprintf(buf, sizeof(buf), "%s%d, %s%d", tr(STR_SECTION_PREFIX), bm.spineIndex + 1, tr(STR_PAGE_PREFIX),
             bm.pageNumber + 1);
  }
  return std::string(buf);
}

// ---- Tabs -------------------------------------------------------------------------------------

void GlobalBookmarksActivity::focusBar() {
  {
    RenderLock lock(*this);
    nav.reset(-1);
  }
  requestUpdate();
}

void GlobalBookmarksActivity::focusList() {
  const int first = firstSelectableIndex();
  if (first < 0) return;  // nothing to select: the focus stays on the bar
  {
    RenderLock lock(*this);
    nav.reset(first);
  }
  buttonNavigator.resync(ButtonNavigator::getStepNextButtons(), ButtonNavigator::getStepPreviousButtons());
  requestUpdate();
}

void GlobalBookmarksActivity::showTab(const LibraryTab tab, const bool barFocus) {
  if (tab == LibraryTab::Bookmarks) return;
  activityManager.goToLibrary(tab, {}, {}, std::nullopt, barFocus);
}

void GlobalBookmarksActivity::tabActionTrampoline(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<GlobalBookmarksActivity*>(user);
  self->app.clearTapFlash();
  self->showTab(libraryTabFrom(static_cast<uint8_t>(event.value)), false);
}

// ---- Actions ----------------------------------------------------------------------------------

void GlobalBookmarksActivity::openSelected() {
  const int selected = nav.selected;
  if (selected < 0 || selected >= listCount() || isSeparatorRow(selected)) return;
  const auto& row = rows[selected];
  const auto& entries = GLOBAL_BOOKMARKS.getEntries();
  if (row.bookIndex >= entries.size()) return;
  const auto& entry = entries[row.bookIndex];
  if (row.bookmarkIndex >= entry.bookmarks.size()) return;
  const auto& bm = entry.bookmarks[row.bookmarkIndex];

  if (!Storage.exists(entry.sourcePath.c_str())) {
    LOG_ERR("GBA", "Source file missing, reconciling: %s", entry.sourcePath.c_str());
    GLOBAL_BOOKMARKS.removeBySourcePath(entry.sourcePath);
    GLOBAL_BOOKMARKS.save();
    {
      RenderLock lock(*this);
      rebuildRows();
      nav.reset(firstSelectableIndex());
    }
    requestUpdate();
    return;
  }

  auto& jump = APP_STATE.pendingBookmarkJump;
  jump.active = true;
  jump.bookPath = entry.sourcePath;
  jump.spineIndex = bm.spineIndex;
  jump.pageNumber = bm.pageNumber;
  APP_STATE.saveToFile();

  LOG_DBG("GBA", "Jumping to bookmark in %s at %u/%u", entry.sourcePath.c_str(), bm.spineIndex, bm.pageNumber);
  ReturnHint hint;
  hint.target = ReturnTo::GlobalBookmarks;
  hint.selectionContext = entry.sourcePath;
  hint.selectBookmarkIndex = static_cast<int>(row.bookmarkIndex);
  activityManager.replaceWithReader(entry.sourcePath, std::move(hint));
}

template <typename Op>
void GlobalBookmarksActivity::mutateBook(size_t bookIndex, Op&& op) {
  const auto& entries = GLOBAL_BOOKMARKS.getEntries();
  if (bookIndex >= entries.size()) return;
  const auto entry = entries[bookIndex];  // copy — index may invalidate after sync

  BookmarkStore store;
  store.load(entry.cacheDir);
  if (!op(store)) return;
  store.save();

  GLOBAL_BOOKMARKS.syncFromStore(store, entry.sourcePath, entry.cacheDir, entry.title, entry.isTxt);
  GLOBAL_BOOKMARKS.save();
}

void GlobalBookmarksActivity::deleteSelected() {
  const int selected = nav.selected;
  if (selected < 0 || selected >= listCount() || isSeparatorRow(selected)) return;
  const auto& row = rows[selected];
  const size_t bookmarkIndex = row.bookmarkIndex;

  mutateBook(row.bookIndex, [bookmarkIndex](BookmarkStore& store) {
    if (bookmarkIndex >= store.getAll().size()) return false;
    store.removeAt(bookmarkIndex);
    return true;
  });

  {
    RenderLock lock(*this);
    rebuildRows();
    const int total = listCount();
    // The row now in its place, else the nearest bookmark before it; none left: the bar.
    int next = std::min(selected, total - 1);
    while (next >= 0 && isSeparatorRow(next)) next = next + 1 < total ? next + 1 : -1;
    if (next < 0) {
      next = std::min(selected, total - 1);
      while (next >= 0 && isSeparatorRow(next)) --next;
    }
    nav.reset(next);
  }
  requestUpdate();
}

void GlobalBookmarksActivity::renameSelected() {
  const int selected = nav.selected;
  if (selected < 0 || selected >= listCount() || isSeparatorRow(selected)) return;
  const auto& row = rows[selected];
  const auto& entries = GLOBAL_BOOKMARKS.getEntries();
  if (row.bookIndex >= entries.size()) return;
  const auto& entry = entries[row.bookIndex];
  if (row.bookmarkIndex >= entry.bookmarks.size()) return;

  const size_t bookIndex = row.bookIndex;
  const size_t bookmarkIndex = row.bookmarkIndex;
  const std::string initial =
      entry.bookmarks[bookmarkIndex].name.empty() ? getRowTitle(selected) : entry.bookmarks[bookmarkIndex].name;

  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RENAME), initial,
                                                                 BookmarkStore::MAX_NAME_LENGTH, InputType::Text),
                         [this, bookIndex, bookmarkIndex](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             const auto& kr = std::get<KeyboardResult>(result.data);
                             mutateBook(bookIndex, [bookmarkIndex, &kr](BookmarkStore& store) {
                               if (bookmarkIndex >= store.getAll().size()) return false;
                               store.rename(bookmarkIndex, kr.text);
                               return true;
                             });
                             RenderLock lock(*this);
                             rebuildRows();
                           }
                           requestUpdate();
                         });
}

void GlobalBookmarksActivity::activateIndex(const int index) {
  app.clearTapFlash();
  nav.selected = index;
  openSelected();
}

// ---- Buttons ----------------------------------------------------------------------------------

bool GlobalBookmarksActivity::handleCustomInput() {
  using Direction = MappedInputManager::Direction;
  ButtonEventManager::ButtonEvent ev;
  while (buttonEvents.consumeEvent(ev)) {
    if (ev.button == MappedInputManager::Button::Back) {
      onGoHome();
      return true;
    }
    if (ev.type != ButtonEventManager::PressType::Short) continue;
    const bool left = MappedInputManager::isDirection(ev.button, Direction::Left);
    const bool right = MappedInputManager::isDirection(ev.button, Direction::Right);
    const bool up = MappedInputManager::isDirection(ev.button, Direction::Up);
    const bool down = MappedInputManager::isDirection(ev.button, Direction::Down);
    const bool confirm = ev.button == MappedInputManager::Button::Confirm;

    if (barFocused()) {
      switch (libraryBarMove(left, right, down, confirm)) {
        case LibraryBarMove::PrevTab:
          showTab(libraryTabStep(LibraryTab::Bookmarks, -1), true);
          return true;
        case LibraryBarMove::NextTab:
          showTab(libraryTabStep(LibraryTab::Bookmarks, 1), true);
          return true;
        case LibraryBarMove::ToList:
          focusList();
          return true;
        case LibraryBarMove::Stay:
          break;
      }
      continue;
    }

    if (up && rows.empty()) {
      focusBar();
      return true;
    }
    (void)down;
    if (confirm) {
      openSelected();
      return true;
    }
    if (left) {
      renameSelected();
      return true;
    }
    if (right) {
      deleteSelected();
      return true;
    }
  }
  return false;
}

void GlobalBookmarksActivity::navigateButtons() {
  const int total = listCount();
  // On the bar the navigator still sees every press (it counts them; a press it missed now would
  // step the list later), but what it does is dropped: the bar's keys are handled above.
  if (barFocused()) {
    int ignored = 0;
    buttonNavigator.onNextList(ButtonNavigator::getStepNextButtons(), ignored, total, [] {});
    buttonNavigator.onPreviousList(ButtonNavigator::getStepPreviousButtons(), ignored, total, [] {});
    return;
  }
  const int previous = nav.selected;
  int selected = previous;
  bool changed = false;
  bool wentUp = false;
  buttonNavigator.onNextList(ButtonNavigator::getStepNextButtons(), selected, total, [&changed] { changed = true; });
  buttonNavigator.onPreviousList(ButtonNavigator::getStepPreviousButtons(), selected, total, [&] {
    changed = true;
    wentUp = true;
  });
  // Up from the first bookmark goes to the tab bar rather than round to the last one.
  if (wentUp && libraryUpToBar(previous, firstSelectableIndex())) {
    focusBar();
    return;
  }
  // Book titles are headings, not stops.
  if (changed && selected < firstSelectableIndex()) selected = firstSelectableIndex();  // a jump to the top
  if (changed && isSeparatorRow(selected)) {
    const int step = wentUp ? -1 : 1;
    for (int tries = 0; tries < total && isSeparatorRow(selected); ++tries)
      selected = (selected + step + total) % total;
  }
  if (changed && selected != previous) moveSelectionTo(selected);
}

// ---- Drawing ----------------------------------------------------------------------------------

void GlobalBookmarksActivity::buildScreen(UiScreen& screen) {
  layoutListArea(screen, 0, 0, /*spacer=*/false);
  FileBrowserActivity::composeLibraryTabBar(screen, LibraryTab::Bookmarks, barFocused(), ACTION_USER);
  screen.spacer(static_cast<int16_t>(UITheme::getInstance().getMetrics().verticalSpacing));

  if (rows.empty()) {
    fui::TextAreaProps empty;
    empty.text = tr(STR_NO_GLOBAL_BOOKMARKS);
    empty.style = screen.theme().bodyText;
    empty.showCaret = false;
    screen.textArea(empty);
    return;
  }

  auto props = listProps(screen);
  props.count = static_cast<uint16_t>(listCount());
  syncListViewport(screen, props);
  const int total = listCount();
  windowFirst = static_cast<uint16_t>(std::max(0, std::min(nav.top, total)));
  windowCount = static_cast<uint16_t>(std::min(total - windowFirst, static_cast<int>(LIST_WINDOW_CAPACITY)));
  for (uint16_t offset = 0; offset < windowCount; ++offset) {
    const int index = windowFirst + offset;
    windowLabels[offset] = getRowTitle(index);
    auto& item = windowItems[offset];
    item = {};
    item.label = windowLabels[offset].c_str();
    item.actionValue = static_cast<int16_t>(index);
    item.enabled = !isSeparatorRow(index);
    item.isHeader = isSeparatorRow(index);
  }
  props.items = windowItems.data();
  props.itemsWindowFirst = windowFirst;
  props.itemsWindowCount = windowCount;
  noteListSelection(props);
  screen.list(props);
}

void GlobalBookmarksActivity::drawChrome() {
  GUI.drawHeader(renderer, UITheme::getHeaderRect(renderer), tr(STR_LIBRARY));
}

void GlobalBookmarksActivity::drawFooter() {
  const bool onBar = nav.selected < 0;
  const bool onBookmark = !onBar && nav.selected < listCount() && !isSeparatorRow(nav.selected);
  const char* confirm = onBar ? (rows.empty() ? "" : tr(STR_SELECT)) : onBookmark ? tr(STR_OPEN) : "";
  const char* left = onBar        ? "\xC2\xAB"
                     : onBookmark ? tr(STR_RENAME)
                                  : "";
  const char* right = onBar        ? "\xC2\xBB"
                      : onBookmark ? tr(STR_DELETE)
                                   : "";
  const auto hints = mappedInput.mapHints(tr(STR_HOME), confirm, left, right, onBar ? "" : tr(STR_DIR_UP),
                                          rows.empty() ? "" : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.front.btn1, hints.front.btn2, hints.front.btn3, hints.front.btn4);
  GUI.drawSideButtonHints(renderer, hints.side.up, hints.side.down);
}
