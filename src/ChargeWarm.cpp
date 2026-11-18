#include "ChargeWarm.h"

#include <Arduino.h>
#include <CooperativeAbort.h>
#include <Epub.h>
#include <Epub/Section.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_system.h>

#include <cstdio>
#include <cstring>
#include <memory>

namespace ChargeWarm {

namespace {
bool readState(State& s) {
  if (!Storage.exists(kStatePath)) return false;
  std::string data;
  if (!Storage.readFileToString("CHW", kStatePath, 512, data)) return false;
  return decode(data, s);
}

void writeState(const State& s) {
  std::string data;
  encode(s, data);
  Storage.mkdir("/.crosspoint");
  HalFile file;
  if (!Storage.openFileForWrite("CHW", kStatePath, file)) return;
  file.write(data.data(), data.size());
  file.close();
}

// CooperativeAbort's predicate takes no context: the pass's own, for its duration.
bool (*gKeepGoing)(void*) = nullptr;
void* gKeepGoingCtx = nullptr;
bool abortWanted() { return gKeepGoing != nullptr && !gKeepGoing(gKeepGoingCtx); }

// With the scratch framebuffer taken: what the decoders need on top (a PNG's 32 KB window and
// scanlines, the pass arena, a page's text) with room to spare.
constexpr uint32_t kMinFreeHeapAfterScratch = 80 * 1024;
}  // namespace

void rememberReader(const std::string& bookPath, const Frame& frame, const std::string& sectionPath, const int spine,
                    const int page, const int spineCount) {
  uint32_t hash = 0;
  if (bookPath.empty() || bookPath.size() > kMaxPathLength || !hashFromSectionPath(sectionPath, hash)) return;
  writeState(fromReader(bookPath, frame, hash, spine, page, spineCount));
}

void prepare(GfxRenderer& renderer, bool (*keepGoing)(void*), void* ctx) {
  State s;
  if (!readState(s) || !pending(s)) return;
  // Moved to /COMPLETED or deleted since: its cache went with it, or is about to.
  if (!Storage.exists(s.bookPath.c_str())) return;

  const Frame& f = s.frame;
  const size_t scratchSize = static_cast<size_t>(f.panelWidthBytes) * f.panelHeight;
  // No pixel is written to it (ScopedCacheOnlyImageWrites below); it is there so a draw this
  // pass was not meant to make lands in memory of its own instead of at address 0.
  std::unique_ptr<uint8_t[]> scratch(new (std::nothrow) uint8_t[scratchSize]);
  if (!scratch || esp_get_free_heap_size() < kMinFreeHeapAfterScratch) {
    LOG_INF("CHW", "Pictures not prepared: not enough memory (free=%lu)",
            static_cast<unsigned long>(esp_get_free_heap_size()));
    return;
  }
  memset(scratch.get(), 0xFF, scratchSize);

  const State before = s;
  const uint32_t start = millis();
  LOG_INF("CHW", "Preparing pictures: %s, spine %d page %u to spine %d (free=%lu)", s.bookPath.c_str(), s.spine,
          static_cast<unsigned>(s.page), s.endSpine, static_cast<unsigned long>(esp_get_free_heap_size()));
  renderer.beginWithoutPanel(f.panelWidth, f.panelHeight, f.panelWidthBytes, scratch.get());
  renderer.setOrientation(static_cast<GfxRenderer::Orientation>(f.orientation));
  gKeepGoing = keepGoing;
  gKeepGoingCtx = ctx;
  CooperativeAbort::setLongTaskAbortPredicate(&abortWanted);
  {
    HalPowerManager::Lock powerLock;  // decoding is CPU work: full clock
    GfxRenderer::ScopedCacheOnlyImageWrites cacheOnly(renderer);
    const std::string cachePath = Epub::cachePathFor(s.bookPath, "/.crosspoint");
    while (pending(s)) {
      char name[32];
      snprintf(name, sizeof(name), "/%d_%08lx.bin", static_cast<int>(s.spine),
               static_cast<unsigned long>(s.sectionHash));
      const auto result = Section::warmImagesInFile(renderer, Epub::spineCacheDir(cachePath, s.spine) + name, s.page,
                                                    f.left, f.top, f.viewportHeight, f.grayscale != 0, keepGoing, ctx);
      if (result == Section::FileWarm::Stopped) break;
      // Done, or no layout for that chapter yet (the reader has not been there): the next one.
      nextSpine(s);
    }
  }
  CooperativeAbort::setLongTaskAbortPredicate(nullptr);
  CooperativeAbort::clearAborted();
  gKeepGoing = nullptr;
  gKeepGoingCtx = nullptr;
  renderer.beginWithoutPanel(0, 0, 0, nullptr);
  if (s.spine != before.spine || s.page != before.page) writeState(s);
  LOG_INF("CHW", "Pictures prepared for %lu ms; next: spine %d page %u (%s)",
          static_cast<unsigned long>(millis() - start), s.spine, static_cast<unsigned>(s.page),
          pending(s) ? "more next charge" : "done");
}

}  // namespace ChargeWarm
