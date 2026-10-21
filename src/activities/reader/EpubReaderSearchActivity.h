#pragma once
#include <Epub.h>
#include <Epub/BookTextSearch.h>

#include <memory>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Full-text search in the open book. Asks for the query on the keyboard, then
// searches one chapter per loop pass — starting at the current chapter and
// wrapping — so results appear while it runs and any button press interrupts
// it at once. Picking a result returns a SearchResult; the reader jumps to it.
//
// Back while searching stops the search and keeps the results so far; Back
// again leaves.
class EpubReaderSearchActivity final : public UiListActivity {
 public:
  EpubReaderSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::shared_ptr<Epub>& epub,
                           int startSpineIndex, std::string* queryStore);
  void onEnter() override;
  void loop() override;
  // While searching: full CPU speed with no idle delay between chapters, and
  // no auto-sleep part-way through a long book.
  bool skipLoopDelay() override { return searching; }
  bool preventAutoSleep() override { return searching; }

 private:
  // Bounds memory on the C3: each hit keeps a short snippet.
  static constexpr size_t MAX_HITS = 80;
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
  std::string* queryStore;  // the reader's last query: prefills the keyboard, kept for next time
  std::string query;
  std::vector<uint32_t> needle;

  bool searching = false;
  int spinesDone = 0;
  int spineCount = 0;
  bool capped = false;
  unsigned long lastRepaintMs = 0;

  std::vector<Entry> entries;
  std::vector<std::string> chapterTitles;
  int lastTitledSpine = -1;
  std::vector<freeink::ui::ListItem> rowItems;
  std::string header;

  void askForQuery();
  void startSearch(const std::string& text);
  void searchNextChapter();
  void stopSearching();
  void rebuildRows();  // caller holds the render lock
  void updateHeader();

  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override { return header.c_str(); }
};
