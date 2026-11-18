#include "CompletedBooks.h"

#include <Arduino.h>
#include <BookCache.h>
#include <CompletedLedger.h>
#include <FolderWalk.h>
#include <FsHelpers.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "BookRelocation.h"
#include "CrossPointState.h"

namespace CompletedBooks {
namespace {

constexpr char kCompletedDir[] = "/COMPLETED";
// In the cache folder: writing it is no change to the card (HalStorage::contentGeneration).
constexpr char kLedgerPath[] = "/.crosspoint/completed_trim.bin";
// 4 bytes a book, twice over while a sweep runs (3 KB). Books past it are left as they are.
constexpr size_t kMaxBooks = 384;
// A sweep runs on the Library's loop, so input waits on it: a first sweep after an update may find
// every finished book still untrimmed, and goes through them a few at a time, Library open by open.
constexpr uint8_t kMaxTrimsPerSweep = 6;
constexpr uint32_t kSweepBudgetMs = 1500;

bool sweptThisBoot = false;
uint32_t sweptAtGeneration = 0;
bool sweepPending_ = false;

bool isBook(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path) || FsHelpers::hasTxtExtension(path) ||
         FsHelpers::hasMarkdownExtension(path);
}

size_t readLedger(uint32_t* keys) {
  // A plain open: the ledger is absent until the first sweep, and openFileForRead logs a miss.
  HalFile f = Storage.open(kLedgerPath);
  if (!f) return 0;
  const int got = f.read(reinterpret_cast<uint8_t*>(keys), kMaxBooks * sizeof(uint32_t));
  f.close();
  size_t count = got > 0 ? static_cast<size_t>(got) / sizeof(uint32_t) : 0;
  // Written sorted; sorted again so a damaged file cannot break the binary search.
  std::sort(keys, keys + count);
  count = static_cast<size_t>(std::unique(keys, keys + count) - keys);
  return count;
}

void writeLedger(const uint32_t* keys, const size_t count) {
  HalFile f;
  if (!Storage.openFileForWrite("CMP", kLedgerPath, f)) return;  // the next sweep trims those books again
  f.write(reinterpret_cast<const uint8_t*>(keys), count * sizeof(uint32_t));
  f.close();
}

struct Sweep {
  const uint32_t* known;
  size_t knownCount;
  uint32_t* seen;
  size_t seenCount = 0;
  uint32_t startMs;
  uint8_t trims = 0;
  bool cut = false;   // budget spent: books left untrimmed for the next sweep
  bool full = false;  // more books than the ledger holds
};

void visit(const std::string& path, void* ctx) {
  auto& s = *static_cast<Sweep*>(ctx);
  if (!isBook(path)) return;
  const uint32_t key = CompletedLedger::pathKey(path);
  if (CompletedLedger::contains(s.known, s.knownCount, key)) {
    if (!CompletedLedger::insert(s.seen, s.seenCount, kMaxBooks, key)) s.full = true;
    return;
  }
  if (s.cut || s.full) return;
  if (s.trims >= kMaxTrimsPerSweep || millis() - s.startMs > kSweepBudgetMs) {
    s.cut = true;
    return;
  }
  ++s.trims;
  if (trim(path) && !CompletedLedger::insert(s.seen, s.seenCount, kMaxBooks, key)) s.full = true;
}

}  // namespace

bool trim(const std::string& path) {
  if (!CompletedLedger::isInCompleted(path)) return false;  // only a finished book gives its cache up
  const std::string cacheDir = BookRelocation::cacheDirFor(path);
  if (cacheDir.empty()) return false;
  // The finished-book flow's sync runs after the reader has closed the book and reads its layout.
  const auto& sync = APP_STATE.koReaderSyncSession;
  if (sync.active && sync.epubPath == path) {
    LOG_DBG("CMP", "%s is being synced; its cache stays for now", path.c_str());
    return false;
  }
  HalPowerManager::Lock powerLock;
  const bool ok = BookCache::clear(cacheDir, BookCache::Keep::Finished);
  if (ok) {
    LOG_DBG("CMP", "Freed the cache of finished %s", path.c_str());
  } else {
    LOG_ERR("CMP", "Could not free all of the cache of %s", path.c_str());
  }
  return ok;
}

void libraryOpened() {
  if (!sweptThisBoot || Storage.contentGeneration() != sweptAtGeneration) sweepPending_ = true;
}

bool sweepPending() { return sweepPending_; }

void sweepIfPending() {
  if (!sweepPending_) return;
  sweepPending_ = false;
  // Read first: a change while the sweep runs makes the next Library open sweep again.
  const uint32_t generation = Storage.contentGeneration();
  sweptAtGeneration = generation;
  sweptThisBoot = true;
  if (!Storage.exists(kCompletedDir)) return;

  auto known = makeUniqueNoThrow<uint32_t[]>(kMaxBooks);
  auto seen = makeUniqueNoThrow<uint32_t[]>(kMaxBooks);
  if (!known || !seen) {
    sweptThisBoot = false;  // tried again at the next Library open
    return;
  }
  Sweep s{known.get(), readLedger(known.get()), seen.get()};
  s.startMs = millis();
  if (!FolderWalk::eachFile(kCompletedDir, visit, &s)) return;

  // A walk that did not see every book keeps the entries it did not see; a complete one forgets
  // the books that left /COMPLETED.
  if (s.cut || s.full) {
    for (size_t i = 0; i < s.knownCount; ++i) CompletedLedger::insert(s.seen, s.seenCount, kMaxBooks, s.known[i]);
  }
  if (s.cut) sweptThisBoot = false;
  if (s.seenCount != s.knownCount || memcmp(s.seen, s.known, s.seenCount * sizeof(uint32_t)) != 0) {
    writeLedger(s.seen, s.seenCount);
  }
  LOG_DBG("CMP", "Sweep: %u trimmed, %u known%s", static_cast<unsigned>(s.trims), static_cast<unsigned>(s.seenCount),
          s.cut ? ", more next time" : "");
}

}  // namespace CompletedBooks
