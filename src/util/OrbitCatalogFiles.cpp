#include "OrbitCatalogFiles.h"

#include <FileReplace.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace OrbitCatalogFiles {

namespace {
std::string serverFile(const std::string& serverUrl, const char* suffix) {
  uint32_t h = 2166136261u;
  for (const unsigned char c : serverUrl) {
    h ^= c;
    h *= 16777619u;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "/.crosspoint/opds_%08lx%s", static_cast<unsigned long>(h), suffix);
  return buf;
}

// Opens `path` and checks its header line. The reader is left after the header.
bool openWithHeader(const std::string& path, const char* kind, HalFile& file, LineReader& reader,
                    OrbitCatalog::Header* header) {
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("OCAT", path, file)) return false;
  std::string_view line;
  uint32_t offset = 0;
  OrbitCatalog::Header h;
  if (!reader.next(line, offset) || !OrbitCatalog::parseHeader(line, kind, h)) {
    file.close();
    return false;
  }
  if (header) *header = h;
  return true;
}
}  // namespace

std::string catalogPath(const std::string& serverUrl) { return serverFile(serverUrl, "_cat.txt"); }
std::string waitingPath(const std::string& serverUrl) { return serverFile(serverUrl, "_wait.txt"); }

bool LineReader::fill() {
  chunkStart += static_cast<uint32_t>(len);
  pos = 0;
  const int n = file.read(chunk, sizeof(chunk));
  len = n > 0 ? static_cast<size_t>(n) : 0;
  return len > 0;
}

bool LineReader::next(std::string_view& out, uint32_t& offset) {
  if (pos >= len && !fill()) return false;
  offset = chunkStart + static_cast<uint32_t>(pos);
  size_t n = 0;
  bool overflow = false;
  for (;;) {
    if (pos >= len && !fill()) break;  // last line without a break
    const char c = chunk[pos++];
    if (c == '\n') break;
    if (n + 1 < cap) {
      line[n++] = c;
    } else {
      overflow = true;
    }
  }
  out = overflow ? std::string_view{} : std::string_view(line, n);
  return true;
}

bool readHeader(const std::string& path, const char* kind, OrbitCatalog::Header& out) {
  HalFile file;
  char lineBuf[160];
  LineReader reader(file, lineBuf, sizeof(lineBuf));
  if (!openWithHeader(path, kind, file, reader, &out)) return false;
  file.close();
  return true;
}

bool forEach(const std::string& path, const char* kind, char* lineBuf, const Visit visit, void* ctx) {
  HalFile file;
  LineReader reader(file, lineBuf, OrbitCatalog::kMaxLine);
  if (!openWithHeader(path, kind, file, reader, nullptr)) return false;
  std::string_view line;
  uint32_t offset = 0;
  OrbitCatalog::RecordView view;
  while (reader.next(line, offset)) {
    if (!OrbitCatalog::splitRecord(line, view)) continue;  // damaged or cut short: skipped
    if (!visit(ctx, offset, view)) break;
  }
  file.close();
  return true;
}

bool readAt(const std::string& path, const uint32_t offset, char* lineBuf, OrbitCatalog::RecordView& out) {
  HalFile file;
  if (!Storage.openFileForRead("OCAT", path, file)) return false;
  bool ok = file.seekSet(offset);
  if (ok) {
    LineReader reader(file, lineBuf, OrbitCatalog::kMaxLine);
    std::string_view line;
    uint32_t ignored = 0;
    ok = reader.next(line, ignored) && OrbitCatalog::splitRecord(line, out);
  }
  file.close();
  return ok;
}

void loadIds(const std::string& path, const char* kind, char* lineBuf, std::vector<uint32_t>& out) {
  out.clear();
  forEach(
      path, kind, lineBuf,
      [](void* ctx, uint32_t, const OrbitCatalog::RecordView& view) {
        auto& ids = *static_cast<std::vector<uint32_t>*>(ctx);
        if (ids.size() >= 8000) return false;  // 32 KB: far past any library this is for
        ids.push_back(OrbitCatalog::idHash(view[OrbitCatalog::kId]));
        return true;
      },
      &out);
  std::sort(out.begin(), out.end());
}

bool replaceWith(const std::string& tmpPath, const std::string& path) {
  if (FileReplace::replace(tmpPath.c_str(), path.c_str())) return true;
  LOG_ERR("OCAT", "Could not put %s in place", path.c_str());
  Storage.remove(tmpPath.c_str());
  return false;
}

int waitingCount(const std::string& serverUrl, char* lineBuf) {
  int count = 0;
  forEach(
      waitingPath(serverUrl), OrbitCatalog::kWaitingKind, lineBuf,
      [](void* ctx, uint32_t, const OrbitCatalog::RecordView&) {
        ++*static_cast<int*>(ctx);
        return true;
      },
      &count);
  return count;
}

bool addWaiting(const std::string& serverUrl, const OrbitCatalog::Book& book, char* lineBuf) {
  const std::string path = waitingPath(serverUrl);
  std::vector<uint32_t> ids;
  loadIds(path, OrbitCatalog::kWaitingKind, lineBuf, ids);
  if (std::binary_search(ids.begin(), ids.end(), OrbitCatalog::idHash(book.id))) return true;  // already there
  if (ids.size() >= kMaxWaiting) return false;
  std::string text;
  if (!OrbitCatalog::appendRecord(book, text)) return false;
  Storage.mkdir("/.crosspoint");
  HalFile file;
  bool ok;
  if (!Storage.exists(path.c_str()) || ids.empty()) {
    // A new list (or one whose header was unreadable): written whole.
    std::string head;
    OrbitCatalog::appendHeader({}, OrbitCatalog::kWaitingKind, head);
    text.insert(0, head);
    ok = Storage.openFileForWrite("OCAT", path, file);
  } else {
    ok = Storage.openFileForUpdate("OCAT", path, file) && file.seekSet(file.size());
  }
  if (ok) {
    ok = file.write(text.data(), text.size()) == text.size();
    ok = file.close() && ok;
  }
  if (!ok) LOG_ERR("OCAT", "Could not add to the waiting list");
  return ok;
}

bool removeWaiting(const std::string& serverUrl, std::string_view id, char* lineBuf) {
  const std::string path = waitingPath(serverUrl);
  const std::string tmp = path + ".tmp";
  HalFile out;
  if (!Storage.openFileForWrite("OCAT", tmp, out)) return false;
  struct Ctx {
    HalFile* out;
    uint32_t drop;
    bool ok;
    std::string text;
  } ctx{&out, OrbitCatalog::idHash(id), true, {}};
  ctx.text.reserve(512);
  OrbitCatalog::appendHeader({}, OrbitCatalog::kWaitingKind, ctx.text);
  forEach(
      path, OrbitCatalog::kWaitingKind, lineBuf,
      [](void* p, uint32_t, const OrbitCatalog::RecordView& view) {
        auto& c = *static_cast<Ctx*>(p);
        if (OrbitCatalog::idHash(view[OrbitCatalog::kId]) == c.drop) return true;
        OrbitCatalog::Book book;
        OrbitCatalog::toBook(view, book);
        OrbitCatalog::appendRecord(book, c.text);
        c.ok = c.out->write(c.text.data(), c.text.size()) == c.text.size() && c.ok;
        c.text.clear();
        return true;
      },
      &ctx);
  if (!ctx.text.empty()) ctx.ok = out.write(ctx.text.data(), ctx.text.size()) == ctx.text.size() && ctx.ok;
  ctx.ok = out.close() && ctx.ok;
  if (!ctx.ok) {
    Storage.remove(tmp.c_str());
    return false;
  }
  return replaceWith(tmp, path);
}

}  // namespace OrbitCatalogFiles
