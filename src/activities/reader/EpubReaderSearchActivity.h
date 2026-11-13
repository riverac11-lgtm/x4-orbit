#pragma once

#include <Epub.h>
#include <Epub/BookTextSearch.h>

#include <memory>
#include <string>
#include <vector>

#include "../Activity.h"
#include "SearchList.h"
#include "util/ButtonNavigator.h"

// Full-text search in the open book. Asks for the query on the keyboard, then searches one
// chapter per loop pass -- starting at the current chapter and wrapping -- so results list
// while it runs, and any button press interrupts the chapter in progress at once (it is
// searched again from its start on the next pass). Picking a result returns a SearchResult;
// the reader jumps to it.
//
// Back while searching stops the search and keeps what was found; Back again leaves.
//
// The list is kept in the book's cache folder when the screen closes (SearchList.h), so the
// next visit opens on it -- same query and hits, the hit last opened selected -- without
// searching again. On the list, Left switches between any match and whole words only (and
// searches again), Right starts a new search: the recent queries when there are any, with
// the keyboard as their first row, otherwise the keyboard at once.
class EpubReaderSearchActivity final : public Activity {
 public:
  EpubReaderSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub,
                           int startSpineIndex, std::string* queryStore, bool* wholeWordStore);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  ListRowTap::Result selectListRow(int index) override;
  bool pageList(ListPageDirection direction) override;
  // While searching: no idle delay between chapters and no auto-sleep mid-book.
  bool skipLoopDelay() override { return searching; }
  bool preventAutoSleep() override { return searching; }

 private:
  // Bounds memory on the C3: each hit keeps a short snippet.
  static constexpr size_t MAX_HITS = searchlist::MAX_HITS;
  // Repaint at most this often while results stream in (e-ink refreshes are slow).
  static constexpr unsigned long REPAINT_INTERVAL_MS = 1500;

  struct Entry {
    uint16_t spineIndex;
    uint16_t titleIndex;  // into chapterTitles
    uint32_t visibleOffset;
    uint32_t visibleEnd;
    std::string snippet;
  };

  std::shared_ptr<Epub> epub;
  int startSpineIndex;
  // The reader's copies of the query and its whole-word choice: they prefill the keyboard, and
  // the reader finds the opened hit on its page by them.
  std::string* queryStore;
  bool* wholeWordStore;
  bool wholeWord = false;
  std::string query;
  std::vector<uint32_t> needle;
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  bool searching = false;
  int spinesDone = 0;
  int spineCount = 0;
  int spinesSkipped = 0;
  bool capped = false;
  unsigned long lastRepaintMs = 0;

  std::vector<Entry> entries;
  std::vector<std::string> chapterTitles;
  int lastTitledSpine = -1;

  // Results: the hit list (also while it fills). Recent: the recent queries, row 0 opens the keyboard.
  enum class View : uint8_t { Results, Recent };
  View view = View::Results;
  bool haveList = false;  // a search ran or a kept list was loaded: Results has something to show
  bool dirty = false;     // the list differs from the kept file
  int16_t openedIndex = -1;
  std::vector<std::string> recent;
  int recentIndex = 0;

  void askForQuery();
  void newSearch();
  bool loadKept();
  void keep(int16_t current);
  void leave();
  void startSearch(const std::string& text);
  void searchNextChapter();
  void cancelAndFinish();
  std::string headerText() const;
};
