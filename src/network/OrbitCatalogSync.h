#pragma once

#include <OpdsParser.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "OpdsServerStore.h"
#include "network/HttpDownloader.h"
#include "util/ChargeSession.h"
#include "util/OrbitCatalogFormat.h"

// The network side of the saved catalog: building and updating it, filling the waiting list from
// the "Send to X4" collection, and fetching the waiting list. Runs on a connected Wi-Fi, from the
// Online Library's update screen or headless while charging (ChargeDelivery). Draws nothing: what
// it is doing goes to the hooks, which may also stop it.
//
// Memory: one kept-alive TLS session for every request; feed pages are parsed as they stream and
// each book goes straight to a temporary file, so no list of books is held. Only the ids of the
// books already in the catalog are (4 bytes each), for the update's stop rule.
class OrbitCatalogSync {
 public:
  enum class Step : uint8_t { Catalog, Collection, Download };
  enum class Result : uint8_t { Ok, Server, Auth, Card, Stopped, NoCatalogFeed };
  struct Hooks {
    void* ctx = nullptr;
    // done/total: books so far (total 0 when unknown); `title` the book in hand, or nullptr.
    void (*status)(void* ctx, Step step, int done, int total, const char* title) = nullptr;
    // Asked often (each book, each page, during downloads): false stops the work.
    bool (*keepGoing)(void* ctx) = nullptr;
    // A transfer's bytes, for a progress bar. Optional.
    void (*bytes)(void* ctx, uint32_t done, uint32_t total) = nullptr;
  };

  OrbitCatalogSync(const OpdsServer& server, const Hooks& hooks);
  ~OrbitCatalogSync();

  // The saved catalog brought up to date: built from "All Books" when there is none (or it is
  // unreadable, or `full`), otherwise the new books from "Recent Books" added. `added` = new books.
  Result updateCatalog(bool full, int& added);
  // Books of the "Send to X4" collection that are neither on the device nor waiting join the
  // waiting list. Ok with added = 0 when the server has no such collection.
  Result collectSendToX4(int& added);
  // Downloads the waiting list (or only the book `onlyId`), removing each book that arrives.
  // Fills summary.arrived / failed / names.
  Result downloadWaiting(ChargeSession::Summary& summary, const std::string& onlyId = {});

  // The TLS connection closed (before heavy card work, or when done).
  void closeSession();

 private:
  struct RootLinks {
    std::string all;
    std::string recent;
    std::string collections;
  };

  bool keepGoing() const { return !hooks.keepGoing || hooks.keepGoing(hooks.ctx); }
  void status(Step step, int done, int total, const char* title) const {
    if (hooks.status) hooks.status(hooks.ctx, step, done, total, title);
  }
  HttpDownloader::Session& session();
  crosspoint::TlsPolicy tlsPolicy() const;
  std::string resolve(const std::string& base, const std::string& href) const;
  Result fetchFeed(const std::string& url, OpdsParser& parser);
  Result loadRoot();
  // Walks an acquisition feed and its rel="next" pages. onBook returns false to stop the walk.
  using BookFn = bool (*)(void* ctx, const OpdsEntry& entry, const std::string& pageUrl);
  Result walkBooks(const std::string& url, BookFn onBook, void* ctx);
  Result buildFull(int& added);
  // walkBooks()' callback for the catalog walks (the context is the .cpp's WriteCtx).
  static bool writeBook(void* ctx, const OpdsEntry& entry, const std::string& pageUrl);
  Result fetchOne(const std::string& id, char* lineBuf, ChargeSession::Summary& summary, bool& arrived);

  const OpdsServer& server;  // an OPDS_STORE entry, which outlives the sync
  Hooks hooks;
  std::unique_ptr<HttpDownloader::Session> net;
  RootLinks root;
  bool rootLoaded = false;
  std::unique_ptr<char[]> lineBuf;
  std::vector<uint32_t> onDevice;
  bool onDeviceScanned = false;

 public:
  // Fills a catalog book from a feed entry: its best download (EPUB first), absolute URLs, series
  // from the metadata or calibre's summary line. False for an entry the reader cannot open.
  static bool bookFromEntry(const OpdsEntry& entry, const std::string& pageUrl, const std::string& serverUrl,
                            uint16_t gen, OrbitCatalog::Book& out);
};
