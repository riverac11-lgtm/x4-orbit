#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "OpdsServerStore.h"
#include "activities/UiListActivity.h"
#include "util/OrbitCatalogFormat.h"

// The Online Library from its saved catalog (OrbitCatalogFiles.h): opens at once, with no network.
//
//   Search / Waiting to download (N) / New since last update (N) / Series / Authors /
//   Browse live server / Update catalog (last updated ...)
//
// Lists of books show each book's state at the right: "On device" (by the browser's file-name
// rule, so earlier downloads count) or "Waiting". Confirm on a book offers Get now / Get when
// charging (or Don't get it); a series lists its books in number order with "Get all missing" on
// top; the waiting list takes a book off with Confirm. Without a saved catalog the screen offers
// to save one, or the live browser. Downloads and updates run in CatalogSyncActivity, which
// reboots on its way out like every Wi-Fi screen.
//
// RAM: the rows of the list on screen as 4-byte file offsets (a names list: 24 bytes a name, up to
// kMaxNames), one line buffer, and the on-device and waiting sets (4 bytes a book).
class OrbitCatalogActivity final : public UiListActivity {
 public:
  OrbitCatalogActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const OpdsServer& server);
  void onEnter() override;

 private:
  enum class View : uint8_t { Root, Books, Names, Actions };
  enum class Kind : uint8_t { Search, Waiting, New, Series, Author, SeriesNames, AuthorNames };
  struct NameRow {
    uint32_t hash;
    uint32_t offset;
    uint16_t count;
    char key[OrbitCatalog::kSortKeyLength];
  };
  static constexpr size_t kMaxRows = 600;
  static constexpr size_t kMaxNames = 600;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  const char* footerConfirmLabel() const override;
  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  bool loadState();
  void showRoot(int selected = 0);
  void showBooks(Kind kind, int selected = 0);
  void showNames(Kind kind, int selected = 0);
  void showActions(int bookRow, bool wholeSet);
  void runSync(uint8_t mode, const std::string& onlyId);
  void startSearch();
  void getMissing(bool now);
  bool bookAt(uint32_t offset, OrbitCatalog::RecordView& view) const;
  const std::string& listPath() const;
  // The row of a book list that is not a book (Get all missing / Get all now), or -1.
  int leadRow() const;
  void setRows(View v, Kind k, int selected);
  bool isWaiting(std::string_view id) const;
  bool onDevice(const OrbitCatalog::RecordView& view) const;
  void reloadWaiting();

  const OpdsServer& server;  // an OPDS_STORE entry, which outlives this screen
  std::string catPath;
  std::string waitPath;
  std::unique_ptr<char[]> lineBuf;
  mutable std::unique_ptr<char[]> rowBuf;  // the render task's own line buffer
  bool haveCatalog = false;
  OrbitCatalog::Header header;
  int newCount = 0;
  int waitingCount = 0;

  View view = View::Root;
  Kind kind = Kind::Search;
  std::vector<uint32_t> rows;  // book offsets in listPath()
  std::vector<NameRow> names;
  std::string query;        // folded search text
  uint32_t nameFilter = 0;  // the series or author whose books are listed
  std::string nameTitle;    // and its name, for the header
  int namesSelected = 0;    // where the names list was, for Back
  int booksSelected = 0;    // where the book list was, for Back from the actions
  int actionRow = -1;       // the book the actions are for, or -1 for the whole series
  int missingCount = 0;
  std::vector<uint32_t> onDeviceSet;
  std::vector<uint32_t> waitingSet;
  std::string actionTitle;

  // Render-task scratch for the row being drawn.
  char labelBuf[128] = {};
  char subBuf[128] = {};
};
