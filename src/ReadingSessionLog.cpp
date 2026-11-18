#include "ReadingSessionLog.h"

#include <HalStorage.h>
#include <Logging.h>

namespace ReadingSessionLog {

bool load(ReadingInsights::SessionLog& out) {
  out = ReadingInsights::SessionLog{};
  FsFile file;
  if (!Storage.openFileForRead("RSL", kPath, file)) return false;  // none yet
  ReadingInsights::SessionLog image;
  const int read = file.read(reinterpret_cast<uint8_t*>(&image), sizeof(image));
  file.close();
  if (read != static_cast<int>(sizeof(image)) || !ReadingInsights::valid(image)) {
    LOG_DBG("RSL", "session log unreadable; starting over");
    return false;
  }
  out = image;
  return true;
}

void append(const std::string& docId, const uint32_t seconds, const uint32_t pages) {
  if (seconds < ReadingInsights::kMinSeconds) return;  // no read of the card for a glance at a book
  ReadingInsights::SessionLog log;
  load(log);
  ReadingInsights::add(log, docId.c_str(), seconds, pages);
  ReadingInsights::seal(log);
  FsFile file;
  if (!Storage.openFileForWrite("RSL", kPath, file)) {
    LOG_ERR("RSL", "session log not written");
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(&log), sizeof(log));
  file.close();
}

}  // namespace ReadingSessionLog
