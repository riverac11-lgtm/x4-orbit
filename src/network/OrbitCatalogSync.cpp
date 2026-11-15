#include "OrbitCatalogSync.h"

#include <Epub.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <OpdsStream.h>
#include <WiFi.h>
#include <Xtc.h>
#include <strings.h>

#include <algorithm>
#include <cstring>

#include "util/OpdsOnDevice.h"
#include "util/OrbitCatalogFiles.h"
#include "util/UrlUtils.h"

using OrbitCatalog::Book;
using Result = OrbitCatalogSync::Result;

namespace {
// Feed pages followed at most, per walk: a loop of rel="next" links must end.
constexpr int kMaxPages = 400;
// The header is written first as a placeholder of this exact size and rewritten in place at the
// end, when the count is known, so the books can stream straight to the file.
constexpr size_t kHeaderBytes = 64;

bool writeAll(HalFile& file, const std::string& text) { return file.write(text.data(), text.size()) == text.size(); }

bool writeHeader(HalFile& file, const OrbitCatalog::Header& header) {
  std::string text;
  OrbitCatalog::appendHeader(header, OrbitCatalog::kCatalogKind, text);
  if (text.size() > kHeaderBytes) return false;
  text.pop_back();  // the '\n', put back after the padding
  text.append(kHeaderBytes - 1 - text.size(), ' ');
  text += '\n';
  return writeAll(file, text);
}

uint32_t nowEpoch() { return static_cast<uint32_t>(HalClock::now()); }

Result fromDownload(const HttpDownloader::DownloadError error) {
  switch (error) {
    case HttpDownloader::OK:
      return Result::Ok;
    case HttpDownloader::AUTH_ERROR:
      return Result::Auth;
    case HttpDownloader::FILE_ERROR:
      return Result::Card;
    case HttpDownloader::ABORTED:
      return Result::Stopped;
    case HttpDownloader::HTTP_ERROR:
      break;
  }
  return Result::Server;
}
}  // namespace

OrbitCatalogSync::OrbitCatalogSync(const OpdsServer& server, const Hooks& hooks)
    : server(server), hooks(hooks), lineBuf(new (std::nothrow) char[OrbitCatalog::kMaxLine]) {}

OrbitCatalogSync::~OrbitCatalogSync() = default;

HttpDownloader::Session& OrbitCatalogSync::session() {
  if (!net) net = std::make_unique<HttpDownloader::Session>();
  return *net;
}

void OrbitCatalogSync::closeSession() { net.reset(); }

crosspoint::TlsPolicy OrbitCatalogSync::tlsPolicy() const {
  // As the browser: a public catalog may fall back to unverified, one with a login never does.
  return server.username.empty() && server.password.empty() ? HttpDownloader::TlsPolicy::VerifiedThenUnverified
                                                            : HttpDownloader::TlsPolicy::Verified;
}

std::string OrbitCatalogSync::resolve(const std::string& base, const std::string& href) const {
  if (href.empty() || href.rfind("http", 0) == 0) return href;
  return UrlUtils::buildUrl(base.empty() ? server.url : base, href);
}

Result OrbitCatalogSync::fetchFeed(const std::string& url, OpdsParser& parser) {
  OpdsParserStream stream{parser};
  const auto error = HttpDownloader::fetchUrl(session(), url, stream, server.username, server.password, tlsPolicy());
  if (error != HttpDownloader::OK) {
    LOG_ERR("OCAT", "Feed %s failed (%d)", url.c_str(), static_cast<int>(error));
    net.reset();  // a broken-off connection is not reused
    return error == HttpDownloader::AUTH_ERROR ? Result::Auth : Result::Server;
  }
  return parser ? Result::Ok : Result::Server;
}

Result OrbitCatalogSync::loadRoot() {
  if (rootLoaded) return Result::Ok;
  if (!lineBuf) return Result::Card;
  OpdsParser parser;
  parser.onEntryParsed = [this](const OpdsEntry& entry) {
    if (entry.type != OpdsEntryType::NAVIGATION) return;
    const std::string href = resolve(server.url, entry.href);
    if (root.all.empty() && OrbitCatalog::matchesRole(entry.title, OrbitCatalog::RootRole::AllBooks)) {
      root.all = href;
    } else if (root.recent.empty() && OrbitCatalog::matchesRole(entry.title, OrbitCatalog::RootRole::Recent)) {
      root.recent = href;
    } else if (root.collections.empty() &&
               OrbitCatalog::matchesRole(entry.title, OrbitCatalog::RootRole::Collections)) {
      root.collections = href;
    }
  };
  const Result result = fetchFeed(server.url, parser);
  if (result != Result::Ok) return result;
  rootLoaded = true;
  LOG_DBG("OCAT", "Root: all=%d recent=%d collections=%d", !root.all.empty(), !root.recent.empty(),
          !root.collections.empty());
  return Result::Ok;
}

Result OrbitCatalogSync::walkBooks(const std::string& url, const BookFn onBook, void* ctx) {
  std::string pageUrl = url;
  bool stop = false;
  for (int page = 0; page < kMaxPages && !pageUrl.empty(); ++page) {
    if (!keepGoing()) return Result::Stopped;
    OpdsParser parser;
    parser.onEntryParsed = [&](const OpdsEntry& entry) {
      if (!stop && !onBook(ctx, entry, pageUrl)) stop = true;
    };
    const Result result = fetchFeed(pageUrl, parser);
    if (result != Result::Ok) return result;
    if (stop) return Result::Ok;
    std::string next = parser.getNextPageUrl().empty() ? std::string{} : resolve(pageUrl, parser.getNextPageUrl());
    if (next == pageUrl) break;
    pageUrl = std::move(next);
  }
  return Result::Ok;
}

bool OrbitCatalogSync::bookFromEntry(const OpdsEntry& entry, const std::string& pageUrl, const std::string& serverUrl,
                                     const uint16_t gen, Book& out) {
  if (entry.type != OpdsEntryType::BOOK || entry.acquisitionLinks.empty()) return false;
  const OpdsAcquisitionLink* link = &entry.acquisitionLinks.front();
  for (const auto& candidate : entry.acquisitionLinks) {
    if (candidate.formatKey == "epub") {
      link = &candidate;
      break;
    }
  }
  const std::string& base = pageUrl.empty() ? serverUrl : pageUrl;
  const auto absolute = [&base](const std::string& href) {
    return href.empty() || href.rfind("http", 0) == 0 ? href : UrlUtils::buildUrl(base, href);
  };
  out.url = absolute(link->href);
  out.id = entry.id.empty() ? out.url : entry.id;
  out.title = entry.title;
  out.author = entry.author;
  out.series = entry.series;
  out.seriesIndex = entry.seriesIndex;
  if (out.series.empty()) OrbitCatalog::seriesFromSummary(entry.summary, out.series, out.seriesIndex);
  out.format = link->formatKey;
  out.updated = entry.updated;
  out.cover = absolute(entry.imageHref);
  out.size = link->length;
  out.gen = gen;
  return true;
}

namespace {
// What a walk that writes books to the catalog file carries.
struct WriteCtx {
  OrbitCatalogSync* sync;
  HalFile* file;
  const std::string* serverUrl;
  OrbitCatalog::MergeStop* stop;  // null for a full build
  uint16_t gen;
  int written = 0;
  bool cardFailed = false;
  bool tooMany = false;
  std::string text;
  Book book;
};
}  // namespace

Result OrbitCatalogSync::buildFull(int& added) {
  added = 0;
  Result result = loadRoot();
  if (result != Result::Ok) return result;
  // No "All Books": the newest list walked to its end has every book too.
  const std::string& feed = root.all.empty() ? root.recent : root.all;
  if (feed.empty()) return Result::NoCatalogFeed;

  const std::string path = OrbitCatalogFiles::catalogPath(server.url);
  const std::string tmp = path + ".tmp";
  Storage.mkdir("/.crosspoint");
  HalFile file;
  if (!Storage.openFileForWrite("OCAT", tmp, file) || !writeHeader(file, {})) {
    if (file) file.close();
    return Result::Card;
  }
  WriteCtx ctx{this, &file, &server.url, nullptr, 1};
  ctx.text.reserve(512);
  status(Step::Catalog, 0, 0, nullptr);
  result = walkBooks(
      feed,
      [](void* p, const OpdsEntry& entry, const std::string& pageUrl) {
        auto& c = *static_cast<WriteCtx*>(p);
        if (!c.sync->keepGoing()) return false;
        if (!OrbitCatalogSync::bookFromEntry(entry, pageUrl, *c.serverUrl, c.gen, c.book)) return true;
        c.text.clear();
        if (!OrbitCatalog::appendRecord(c.book, c.text)) return true;
        if (!writeAll(*c.file, c.text)) {
          c.cardFailed = true;
          return false;
        }
        if (++c.written % 25 == 0) c.sync->status(Step::Catalog, c.written, 0, nullptr);
        return true;
      },
      &ctx);
  if (result == Result::Ok && !keepGoing()) result = Result::Stopped;
  if (result == Result::Ok && ctx.cardFailed) result = Result::Card;
  if (result == Result::Ok) {
    const uint32_t now = nowEpoch();
    if (!file.seekSet(0) || !writeHeader(file, {static_cast<uint32_t>(ctx.written), 1, now, now})) {
      result = Result::Card;
    }
  }
  if (!file.close() && result == Result::Ok) result = Result::Card;
  if (result != Result::Ok) {
    Storage.remove(tmp.c_str());
    return result;
  }
  if (!OrbitCatalogFiles::replaceWith(tmp, path)) return Result::Card;
  added = ctx.written;
  LOG_INF("OCAT", "Catalog built: %d books", ctx.written);
  return Result::Ok;
}

Result OrbitCatalogSync::updateCatalog(const bool full, int& added) {
  added = 0;
  if (!lineBuf) return Result::Card;
  const std::string path = OrbitCatalogFiles::catalogPath(server.url);
  OrbitCatalog::Header header;
  if (full || !OrbitCatalogFiles::readHeader(path, OrbitCatalog::kCatalogKind, header)) return buildFull(added);

  Result result = loadRoot();
  if (result != Result::Ok) return result;
  if (root.recent.empty()) {
    LOG_DBG("OCAT", "No recent-books feed: rebuilding instead");
    return buildFull(added);
  }

  std::vector<uint32_t> known;
  OrbitCatalogFiles::loadIds(path, OrbitCatalog::kCatalogKind, lineBuf.get(), known);
  OrbitCatalog::MergeStop stop(known);
  const std::string tmp = path + ".tmp";
  HalFile file;
  if (!Storage.openFileForWrite("OCAT", tmp, file) || !writeHeader(file, {})) {
    if (file) file.close();
    return Result::Card;
  }
  const uint16_t gen = static_cast<uint16_t>(header.gen + 1);
  WriteCtx ctx{this, &file, &server.url, &stop, gen};
  status(Step::Catalog, 0, 0, nullptr);
  result = walkBooks(
      root.recent,
      [](void* p, const OpdsEntry& entry, const std::string& pageUrl) {
        auto& c = *static_cast<WriteCtx*>(p);
        if (!c.sync->keepGoing()) return false;
        if (!OrbitCatalogSync::bookFromEntry(entry, pageUrl, *c.serverUrl, c.gen, c.book)) return true;
        switch (c.stop->offer(c.book.id)) {
          case OrbitCatalog::MergeStop::Verdict::Known:
            return true;
          case OrbitCatalog::MergeStop::Verdict::Stop:
            return false;
          case OrbitCatalog::MergeStop::Verdict::TooMany:
            c.tooMany = true;
            return false;
          case OrbitCatalog::MergeStop::Verdict::New:
            break;
        }
        c.text.clear();
        if (!OrbitCatalog::appendRecord(c.book, c.text)) return true;
        if (!writeAll(*c.file, c.text)) {
          c.cardFailed = true;
          return false;
        }
        ++c.written;
        c.sync->status(Step::Catalog, c.written, 0, c.book.title.c_str());
        return true;
      },
      &ctx);
  known.clear();
  known.shrink_to_fit();
  if (result == Result::Ok && !keepGoing()) result = Result::Stopped;
  if (result == Result::Ok && ctx.cardFailed) result = Result::Card;
  if (result != Result::Ok || ctx.tooMany) {
    file.close();
    Storage.remove(tmp.c_str());
    if (result == Result::Ok) return buildFull(added);  // too far behind: start over
    return result;
  }

  // The books already there follow the new ones. Card work only from here: TLS goes first.
  closeSession();
  struct CopyCtx {
    HalFile* file;
    std::string text;
    uint32_t count = 0;
    bool ok = true;
  } copy{&file};
  copy.text.reserve(512);
  const bool read = OrbitCatalogFiles::forEach(
      path, OrbitCatalog::kCatalogKind, lineBuf.get(),
      [](void* p, uint32_t, const OrbitCatalog::RecordView& view) {
        auto& c = *static_cast<CopyCtx*>(p);
        Book book;
        OrbitCatalog::toBook(view, book);
        c.text.clear();
        if (!OrbitCatalog::appendRecord(book, c.text)) return true;
        if (!writeAll(*c.file, c.text)) {
          c.ok = false;
          return false;
        }
        ++c.count;
        return true;
      },
      &copy);
  bool ok = read && copy.ok;
  if (ok) {
    ok = file.seekSet(0) &&
         writeHeader(file, {copy.count + static_cast<uint32_t>(ctx.written), gen, header.fullTime, nowEpoch()});
  }
  ok = file.close() && ok;
  if (!ok) {
    Storage.remove(tmp.c_str());
    return Result::Card;
  }
  if (!OrbitCatalogFiles::replaceWith(tmp, path)) return Result::Card;
  added = ctx.written;
  LOG_INF("OCAT", "Catalog updated: %d new, %lu in all", ctx.written,
          static_cast<unsigned long>(copy.count + ctx.written));
  return Result::Ok;
}

Result OrbitCatalogSync::collectSendToX4(int& added) {
  added = 0;
  if (!lineBuf) return Result::Card;
  Result result = loadRoot();
  if (result != Result::Ok) return result;
  if (root.collections.empty()) return Result::Ok;

  // The collection's own feed: a navigation entry of the Collections list (which may be paged).
  struct FindCtx {
    OrbitCatalogSync* sync;
    std::string href;
  } find{this, {}};
  std::string pageUrl = root.collections;
  for (int page = 0; page < 20 && find.href.empty() && !pageUrl.empty(); ++page) {
    OpdsParser parser;
    parser.onEntryParsed = [&](const OpdsEntry& entry) {
      if (find.href.empty() && entry.type == OpdsEntryType::NAVIGATION && OrbitCatalog::isSendCollection(entry.title)) {
        find.href = resolve(pageUrl, entry.href);
      }
    };
    result = fetchFeed(pageUrl, parser);
    if (result != Result::Ok) return result;
    const std::string next =
        parser.getNextPageUrl().empty() ? std::string{} : resolve(pageUrl, parser.getNextPageUrl());
    pageUrl = next == pageUrl ? std::string{} : next;
  }
  if (find.href.empty()) {
    LOG_DBG("OCAT", "No \"Send to X4\" collection");
    return Result::Ok;
  }

  if (!onDeviceScanned) {
    OpdsOnDevice::scan(onDevice);
    onDeviceScanned = true;
  }
  struct AddCtx {
    OrbitCatalogSync* sync;
    int added = 0;
    bool full = false;
    Book book;
  } ctx{this};
  status(Step::Collection, 0, 0, nullptr);
  result = walkBooks(
      find.href,
      [](void* p, const OpdsEntry& entry, const std::string& pageUrl) {
        auto& c = *static_cast<AddCtx*>(p);
        OrbitCatalogSync& s = *c.sync;
        if (!s.keepGoing()) return false;
        if (!OrbitCatalogSync::bookFromEntry(entry, pageUrl, s.server.url, 0, c.book)) return true;
        if (OpdsOnDevice::contains(s.onDevice, c.book.author, c.book.title)) return true;
        // addWaiting() leaves a book already waiting where it is.
        if (!OrbitCatalogFiles::addWaiting(s.server.url, c.book, s.lineBuf.get())) {
          c.full = true;
          return false;
        }
        ++c.added;
        return true;
      },
      &ctx);
  added = ctx.added;
  return result;
}

Result OrbitCatalogSync::fetchOne(const std::string& id, char* buf, ChargeSession::Summary& summary, bool& arrived) {
  arrived = false;
  const std::string waitPath = OrbitCatalogFiles::waitingPath(server.url);
  // Find the book's line again (the list is rewritten after each arrival, so offsets do not last).
  struct FindCtx {
    uint32_t want;
    Book book;
    bool found = false;
  } find{OrbitCatalog::idHash(id)};
  OrbitCatalogFiles::forEach(
      waitPath, OrbitCatalog::kWaitingKind, buf,
      [](void* p, uint32_t, const OrbitCatalog::RecordView& view) {
        auto& f = *static_cast<FindCtx*>(p);
        if (OrbitCatalog::idHash(view[OrbitCatalog::kId]) != f.want) return true;
        OrbitCatalog::toBook(view, f.book);
        f.found = true;
        return false;
      },
      &find);
  if (!find.found) return Result::Ok;
  const Book& book = find.book;

  if (OpdsOnDevice::contains(onDevice, book.author, book.title)) {
    OrbitCatalogFiles::removeWaiting(server.url, book.id, buf);  // arrived some other way
    return Result::Ok;
  }
  const std::string path = OpdsOnDevice::downloadPath(book.author, book.title, OrbitCatalog::extensionFor(book.format));
  LOG_DBG("OCAT", "Fetching %s -> %s", book.title.c_str(), path.c_str());
  const auto progress = [this](const unsigned int done, const unsigned int total) {
    if (hooks.bytes) hooks.bytes(hooks.ctx, done, total);
    return keepGoing();
  };
  const auto error = HttpDownloader::downloadToFile(session(), book.url, path, progress, server.username,
                                                    server.password, tlsPolicy());
  if (error != HttpDownloader::OK) {
    net.reset();
    return fromDownload(error);
  }
  OpdsOnDevice::add(onDevice, path);
  arrived = true;
  summary.addName(book.title);

  const bool isEpub = book.format == "epub" || book.format == "kepub";
  if (isEpub && !book.cover.empty() && keepGoing()) {
    // The cover beside the book, as the browser saves it, for the home screen and the library.
    std::string sidecar = path.substr(0, path.find_last_of('.'));
    const size_t n = book.cover.size();
    sidecar += n >= 4 && strcasecmp(book.cover.c_str() + n - 4, ".png") == 0 ? ".png" : ".jpg";
    if (HttpDownloader::downloadToFile(session(), book.cover, sidecar, nullptr, server.username, server.password,
                                       tlsPolicy()) != HttpDownloader::OK) {
      Storage.remove(sidecar.c_str());
      net.reset();
    }
  }
  // A book downloaded again must not open on the old copy's cache.
  if (isEpub) {
    Epub(path, "/.crosspoint").clearCache();
  } else if (book.format == "xtc" || book.format == "xtch") {
    Xtc(path, "/.crosspoint").clearCache();
  }
  OrbitCatalogFiles::removeWaiting(server.url, book.id, buf);
  return Result::Ok;
}

Result OrbitCatalogSync::downloadWaiting(ChargeSession::Summary& summary, const std::string& onlyId) {
  if (!lineBuf) return Result::Card;
  if (!onDeviceScanned) {
    OpdsOnDevice::scan(onDevice);
    onDeviceScanned = true;
  }
  // The ids to fetch, read once; each is looked up again when its turn comes.
  struct IdsCtx {
    std::vector<std::string> ids;
    const std::string* only;
  } ctx{{}, &onlyId};
  OrbitCatalogFiles::forEach(
      OrbitCatalogFiles::waitingPath(server.url), OrbitCatalog::kWaitingKind, lineBuf.get(),
      [](void* p, uint32_t, const OrbitCatalog::RecordView& view) {
        auto& c = *static_cast<IdsCtx*>(p);
        if (!c.only->empty() && view[OrbitCatalog::kId] != *c.only) return true;
        c.ids.emplace_back(view[OrbitCatalog::kId]);
        return c.ids.size() < OrbitCatalogFiles::kMaxWaiting;
      },
      &ctx);

  const int total = static_cast<int>(ctx.ids.size());
  Result result = Result::Ok;
  for (int i = 0; i < total; ++i) {
    if (!keepGoing()) {
      result = Result::Stopped;
      break;
    }
    if (WiFi.status() != WL_CONNECTED) {
      // Every book left would wait out its own timeout: counted failed (still waiting) instead.
      LOG_ERR("OCAT", "Wi-Fi lost, %d books not fetched", total - i);
      summary.failed = static_cast<uint8_t>(std::min(255, summary.failed + total - i));
      result = Result::Server;
      break;
    }
    status(Step::Download, i, total, nullptr);
    bool arrived = false;
    const Result one = fetchOne(ctx.ids[static_cast<size_t>(i)], lineBuf.get(), summary, arrived);
    if (arrived) {
      if (summary.arrived < 255) ++summary.arrived;
    } else if (one != Result::Ok) {
      if (summary.failed < 255) ++summary.failed;
    }
    if (one == Result::Stopped || one == Result::Auth || one == Result::Card) {
      result = one;
      break;
    }
    if (one != Result::Ok) result = one;  // a book that failed: the rest still go
  }
  return result;
}
