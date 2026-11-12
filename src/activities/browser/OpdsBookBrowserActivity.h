#pragma once
#include <OpdsParser.h>
#include <TlsPolicy.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Activity.h"
#include "../ListController.h"
#include "OpdsBackTo.h"
#include "OpdsServerStore.h"
#include "network/HttpDownloader.h"
#include "util/OpdsDownloadQueue.h"

/**
 * Activity for browsing and downloading books from an OPDS server.
 * Supports navigation through catalog hierarchy and downloading EPUBs.
 */
class OpdsBookBrowserActivity final : public Activity {
 public:
  enum class BrowserState {
    CHECK_WIFI,
    WIFI_SELECTION,
    LOADING,
    BROWSING,
    BOOK_DETAIL,
    FORMAT_SELECTION,
    DOWNLOADING,
    ERROR,
    SEARCH_INPUT
  };

  explicit OpdsBookBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, OpdsServer server,
                                   OpdsBackTo backTo, std::string initialQuery = {})
      : Activity("OpdsBookBrowser", renderer, mappedInput),
        server(std::move(server)),
        backTo(backTo),
        initialQuery_(std::move(initialQuery)) {}

  void onEnter() override;
  bool usesWifi() const override { return true; }
  void onExit() override;
  // Tap on a row -> move the selection there; ActivityManager synthesizes Confirm. Serves
  // BOTH of this screen's lists (the entry list and the format picker), keyed on state.
  ListRowTap::Result selectListRow(int index) override;
  // A swipe pages whichever list is on screen; it never reaches Left/Right (Search and Info).
  bool pageList(ListPageDirection direction) override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  BrowserState state = BrowserState::LOADING;
  std::vector<uint32_t> entryOffsets;
  std::vector<std::string> navigationHistory;
  std::string currentPath;
  std::string searchTemplate;
  bool memoryTrimmed = false;
  // One kept-alive TLS connection for the whole catalog visit: feeds, next pages, searches,
  // covers and the book itself reuse it (the handshake through a tunnel costs seconds on the C3).
  // Dropped before a downloaded book is opened, whose indexing wants the heap back.
  std::unique_ptr<HttpDownloader::Session> netSession;

  // "On device": the download folder, scanned once per visit (on the first feed that lists books)
  // into sorted 32-bit hashes of the book file names without extension -- about 2 KB per 500.
  std::vector<uint32_t> onDeviceHashes;
  bool onDeviceScanned = false;
  void scanOnDeviceBooks();
  bool isOnDevice(const OpdsEntry& book) const;
  void markOnDevice(const std::string& path);
  HttpDownloader::Session& session();
  bool coverAvailable = false;
  int selectorIndex = 0;
  int selectedBookIndex = -1;
  int formatSelectorIndex = 0;
  std::vector<std::string> formatSelectionLabels;
  std::string errorMessage;
  std::string statusMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  // "Format not supported" over the list or the book detail, after Confirm on a book the catalog
  // offers only in formats the reader cannot open. The next press, tap or swipe only takes it down.
  bool formatNotice = false;
  // What the notice says when it is not "Format not supported": the queue's summary, a full list.
  std::string noticeText;
  void showNotice(const char* text);

  // Remember where you were: the feed, the way back and the row of the last visit to this server,
  // kept in /.crosspoint and reopened by onEnter(). Not for a visit opened on a search of its own
  // (a finished book's "more by this author"), which neither reads nor replaces it.
  bool rememberPosition = false;
  // The feed being loaded is the remembered one: a failure opens the root instead of the error.
  bool restoring = false;
  int restoreRow = 0;
  // The remembered feed could not be reached this time. Leave the file alone on the way out, unless
  // the user browses on or backs out of the root.
  bool keepSavedPosition = false;
  bool loadSavedPosition();
  void savePosition() const;
  // The root from the per-server discovery cache, without the network. False when there is none.
  bool showRootFromDiscovery();
  // fetchFeed() without the remembered-position fallback. False with the error screen set up;
  // `unreachable` then says the feed could not be fetched at all (as opposed to read).
  bool loadFeed(const std::string& path, bool& unreachable);

  // Download queue: books marked in the lists (hold Confirm), fetched one after another by Confirm
  // on a marked one. Kept across pages and feeds for the visit; each mark's download details are a
  // small file on the card (see OpdsDownloadQueue.h), so RAM holds 4 bytes per mark.
  OpdsMarks marks;
  bool stopRequested = false;  // Back was pressed during a transfer
  uint32_t lastProgressMs = 0;
  bool isMarked(const OpdsEntry& book) const;
  void toggleMark(const OpdsEntry& book);
  void unmark(uint32_t key);
  void runQueue();
  // One book to the download folder, with its cover and cache clean-up. Draws the progress under
  // statusMessage; the caller sets the state before and after.
  HttpDownloader::DownloadError fetchBook(const OpdsQueueItem& item, bool keepSession);
  bool onTransferProgress(unsigned int downloaded, unsigned int total);
  std::string resolveUrl(const std::string& href) const;

  OpdsServer server;  // Copied at construction — safe even if the store changes during browsing
  OpdsBackTo backTo;
  // Set by leave() on its way to File Transfer, so onExit()'s reboot lands there and not on Home.
  bool leavingToFileTransfer = false;
  std::string initialQuery_;

  OpdsEntry getEntry(size_t index) const;
  void drawFormatNotice() const;
  // The TLS policy of every request to this catalog (see the definition).
  crosspoint::TlsPolicy catalogTlsPolicy() const;
  void dismissFormatNotice();

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void fetchFeed(const std::string& path);
  void navigateToEntry(const OpdsEntry& entry);
  void navigateBack();
  // Back from the top of the catalog: to backTo.
  void leave();
  void downloadBook(const OpdsEntry& book, const OpdsAcquisitionLink& acquisition);
  void chooseBookFormat(const OpdsEntry& book);
  void fetchOsdTemplate(const std::string& osdUrl);
  void launchSearch();
  void performSearch(const std::string& query);
  void fetchCoverForEntry(const OpdsEntry& entry);
  bool preventAutoSleep() override;

  int catalogRowsPerPage() const;
  // Rows per page as the last render drew them. Paging runs on the loop task, where the renderer's
  // orientation can be mid-flip to Portrait for a hint strip, so input reads what render published.
  std::atomic<int> catalogPageRows{1};
  std::atomic<int> formatPageRows{1};
  // The top row of the page the last render drew, published for listWindow().
  std::atomic<int> catalogTop{0};
  std::atomic<int> formatTop{0};
  void showBookDetail(const OpdsEntry& entry);
  void closeFormatPicker();

  // Short Left/Right on the catalog: Search where the feed offers one, Info on a book, and never a
  // step (an action that does not apply leaves its side doing nothing). The format picker keeps
  // the default pair.
  // A long Confirm marks a book for the download queue (on any other row it opens, as a short one).
  static constexpr ListDeclaration kCatalogDeclaration{{true, StrId::STR_SEARCH}, {true, StrId::STR_INFO}, true};
  static constexpr ListDeclaration kFormatDeclaration{};

  struct CatalogHost final : ListHost {
    explicit CatalogHost(OpdsBookBrowserActivity& browser) : browser(browser) {}
    int listCount() const override;
    ListWindow listWindow() const override;
    bool listPagesAligned() const override { return true; }
    int listSelected() const override;
    void listSelect(int row) override;
    bool listActionAvailable(ListGrammar::Side side, int row) const override;
    void onListActivate(int row, bool longPress) override;
    void onListBack() override;
    void onListHome() override;
    void onListAction(ListGrammar::Side side, int row) override;
    OpdsBookBrowserActivity& browser;
  };

  struct FormatHost final : ListHost {
    explicit FormatHost(OpdsBookBrowserActivity& browser) : browser(browser) {}
    int listCount() const override;
    ListWindow listWindow() const override;
    bool listPagesAligned() const override { return true; }
    int listSelected() const override;
    void listSelect(int row) override;
    void onListActivate(int row, bool longPress) override;
    void onListBack() override;
    void onListHome() override;
    OpdsBookBrowserActivity& browser;
  };

  // Each controller below holds a reference to its host.
  CatalogHost catalogHost{*this};
  FormatHost formatHost{*this};
  ListController catalogList{mappedInput, buttonEvents, catalogHost, kCatalogDeclaration};
  ListController formatList{mappedInput, buttonEvents, formatHost, kFormatDeclaration};
};
