#include "BookCache.h"

#include <HalStorage.h>
#include <HalSystem.h>
#include <Logging.h>

#include "FsHelpers.h"

namespace BookCache {

// Not kosync_push.bin: our last push, kept only to spare the sync screen a chapter inflate, and it
// names its place by page number in the layout that is being cleared.
bool isReadingState(const std::string_view name) {
  return name == "progress.bin" || name == "linkstack.bin" || name == "bookmarks.bin" ||
         name == "koreader_syncmethod.txt";
}

bool isKeptWhenFinished(const std::string_view name) {
  if (isReadingState(name)) return true;
  // pagelist.bin (printed page numbers) is written only by the book.bin build, so it stays with it.
  if (name == "book.bin" || name == "pagelist.bin" || name == "details.bin" || name == "fingerprint.bin" ||
      name == "toc.retry" || name == "koreader_docid.txt" || name == "index.bin") {
    return true;
  }
  // cover.bmp / cover_crop.bmp (sleep screen) and thumb_<h>.bmp / thumb_<w>x<h>.bmp (Home, Library),
  // 0-byte "no cover" sentinels included. Not every BMP: a book's own BMP images are extracted here.
  const bool coverOrThumb = name.substr(0, 6) == "thumb_" || name.substr(0, 5) == "cover";
  return coverOrThumb && FsHelpers::hasBmpExtension(name);
}

bool isCoverImage(const std::string_view name) {
  // cover.img is the cover image as the book stores it, before it is converted to a BMP.
  return FsHelpers::hasBmpExtension(name) || name == "cover.img";
}

bool clear(const std::string& cacheDir, const Keep keep, const bool keepCovers) {
  if (!Storage.exists(cacheDir.c_str())) return true;

  if (keep == Keep::Nothing && !keepCovers) {
    if (Storage.removeDir(cacheDir.c_str())) return true;
    LOG_ERR("BKC", "Failed to remove %s", cacheDir.c_str());
    return false;
  }

  HalFile dir = Storage.open(cacheDir.c_str());
  if (!dir || !dir.isDirectory()) {
    LOG_ERR("BKC", "Failed to open %s for a selective clear", cacheDir.c_str());
    if (dir) dir.close();
    return false;
  }

  char name[128];
  bool allRemoved = true;
  bool anyKept = false;
  for (HalFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    HalSystem::feedWatchdog();
    entry.getName(name, sizeof(name));
    const bool isDir = entry.isDirectory();
    entry.close();
    if ((keep == Keep::ReadingState && isReadingState(name)) || (keep == Keep::Finished && isKeptWhenFinished(name)) ||
        (keepCovers && isCoverImage(name))) {
      anyKept = true;
      continue;
    }
    // A subdirectory (spines/, img/, fonts/) goes as a whole: a plain file remove refuses one.
    const std::string path = cacheDir + "/" + name;
    if (!(isDir ? Storage.removeDir(path.c_str()) : Storage.remove(path.c_str()))) {
      LOG_ERR("BKC", "Failed to remove %s", path.c_str());
      allRemoved = false;
    }
  }
  dir.close();

  // A book never opened has no reading state, and a folder with nothing in it is no use to keep.
  if (!anyKept && !Storage.removeDir(cacheDir.c_str())) {
    LOG_ERR("BKC", "Failed to remove %s", cacheDir.c_str());
    allRemoved = false;
  }
  return allRemoved;
}

}  // namespace BookCache
