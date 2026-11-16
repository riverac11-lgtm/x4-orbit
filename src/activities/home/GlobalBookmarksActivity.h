#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../UiListActivity.h"
#include "GlobalBookmarkIndex.h"
#include "LibraryTab.h"

// The Library's Bookmarks tab: the bookmarks of every indexed book, grouped under the book's title,
// and Confirm jumps straight into the book at that place. Left renames a bookmark, Right deletes it.
//
// Data source: GlobalBookmarkIndex (persisted at /.crosspoint/global_bookmarks.bin), reconciled
// against the card on entry (entries whose file has gone are dropped).
//
// It is its own screen beside the file browser that shows the other Library tabs: the same tab bar
// at the top (FileBrowserActivity::composeLibraryTabBar) and the same focus rules -- Up from the
// first bookmark moves focus onto the bar, where Left/Right change tab and Down/Confirm come back.
// Changing to another tab replaces this screen with the browser on that tab.
class GlobalBookmarksActivity final : public UiListActivity {
 public:
  bool drawsBookText() const override { return true; }
  explicit GlobalBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ReturnHint restoreHint = {},
                                   bool tabBarFocus = false)
      : UiListActivity("GlobalBookmarks", renderer, mappedInput),
        restoreHint(std::move(restoreHint)),
        tabBarOnEnter(tabBarFocus) {}

  void onEnter() override;
  void onExit() override;

 private:
  struct Row {
    bool isSeparator = false;
    size_t bookIndex = 0;      // index into GlobalBookmarkIndex entries
    size_t bookmarkIndex = 0;  // index within that entry's bookmarks (separator: ignored)
  };

  std::vector<Row> rows;
  std::optional<GlobalBookmarkIndex::Hold> indexHold;  // from onEnter to onExit: rows index into getEntries()
  ReturnHint restoreHint;
  bool tabBarOnEnter = false;

  static constexpr size_t LIST_WINDOW_CAPACITY = 24;
  std::array<std::string, LIST_WINDOW_CAPACITY> windowLabels;
  std::array<freeink::ui::ListItem, LIST_WINDOW_CAPACITY> windowItems;
  uint16_t windowFirst = 0;
  uint16_t windowCount = 0;

  void rebuildRows();
  std::string getRowTitle(int index) const;
  bool isSeparatorRow(int index) const;
  int firstSelectableIndex() const;
  [[nodiscard]] bool barFocused() { return nav.selected < 0; }
  void focusBar();
  void focusList();
  void showTab(LibraryTab tab, bool barFocus);
  static void tabActionTrampoline(const freeink::ui::ActionEvent& event, void* user);

  void openSelected();
  void deleteSelected();
  void renameSelected();

  // Apply a mutation to the underlying per-book BookmarkStore + global index.
  // `op` is invoked with the loaded store; it should mutate and return true
  // when something changed worth persisting. Title/cacheDir/isTxt are taken
  // from the current index entry.
  template <typename Op>
  void mutateBook(size_t bookIndex, Op&& op);

  int listCount() const override { return static_cast<int>(rows.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  [[nodiscard]] bool isRowSelectable(int index) const override { return !isSeparatorRow(index); }
  bool handleCustomInput() override;
  void navigateButtons() override;
  void drawChrome() override;
  void drawFooter() override;
};
