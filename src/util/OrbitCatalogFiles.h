#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "OrbitCatalogFormat.h"

// The saved catalog and the waiting list on the SD card (formats: OrbitCatalogFormat.h). One pair
// per server, in /.crosspoint, named by the FNV-1a of the server URL like the browser's own files:
//   opds_<hash>_cat.txt    the catalog
//   opds_<hash>_wait.txt   books waiting to be downloaded while charging (or now)
// Every file is read as a stream of lines; a rewrite goes to <file>.tmp and replaces the file
// only when complete (FileReplace), so a power cut or a full card never leaves half a catalog.
namespace OrbitCatalogFiles {

std::string catalogPath(const std::string& serverUrl);
std::string waitingPath(const std::string& serverUrl);
// The last charge-time delivery's outcome (ChargeSession::Summary), one file for the reader.
constexpr char kSummaryPath[] = "/.crosspoint/orbit_delivery.txt";
constexpr size_t kMaxWaiting = 100;

// Lines of a file, through a small chunk buffer, into the caller's line buffer. A line longer than
// that buffer is skipped whole (reported as an empty line).
class LineReader {
 public:
  LineReader(HalFile& file, char* line, size_t lineCapacity) : file(file), line(line), cap(lineCapacity) {}
  // False at the end. `offset` is where the line starts in the file.
  bool next(std::string_view& out, uint32_t& offset);

 private:
  bool fill();
  HalFile& file;
  char* line;
  size_t cap;
  char chunk[256];
  size_t pos = 0;
  size_t len = 0;
  uint32_t chunkStart = 0;
};

bool readHeader(const std::string& path, const char* kind, OrbitCatalog::Header& out);
// Calls visit(ctx, offset, view) for each book line until it returns false. False when the file
// cannot be opened or its header is not `kind`. `lineBuf` must hold OrbitCatalog::kMaxLine bytes.
using Visit = bool (*)(void* ctx, uint32_t offset, const OrbitCatalog::RecordView& view);
bool forEach(const std::string& path, const char* kind, char* lineBuf, Visit visit, void* ctx);
// One book line at a known offset.
bool readAt(const std::string& path, uint32_t offset, char* lineBuf, OrbitCatalog::RecordView& out);
// idHash() of every book in the file, sorted (empty when there is no file).
void loadIds(const std::string& path, const char* kind, char* lineBuf, std::vector<uint32_t>& out);

// The waiting list. Add is an append (the file is created with its header); false when the list
// is full or the card refuses. Remove rewrites the list without the book.
bool addWaiting(const std::string& serverUrl, const OrbitCatalog::Book& book, char* lineBuf);
bool removeWaiting(const std::string& serverUrl, std::string_view id, char* lineBuf);
int waitingCount(const std::string& serverUrl, char* lineBuf);

// Writes a file whole through <path>.tmp. False leaves the old file as it was.
bool replaceWith(const std::string& tmpPath, const std::string& path);

}  // namespace OrbitCatalogFiles
