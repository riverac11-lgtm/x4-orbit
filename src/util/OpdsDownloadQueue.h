#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// The OPDS browser's download queue: the books the user marked in the lists, to be fetched one
// after another. Pure bookkeeping, no SD card and no Arduino (host tests: test/opds_browse).
//
// RAM holds one 32-bit key per mark (to draw the check marks and keep the order); what a download
// needs is one small record per mark on the SD card, read back one at a time while the queue runs.

// What one queued download needs. `coverUrl` may be empty.
struct OpdsQueueItem {
  std::string title;
  std::string author;
  std::string url;
  std::string extension;
  std::string mimeType;
  std::string formatKey;
  std::string coverUrl;
};

namespace OpdsQueue {
constexpr size_t kMaxMarks = 20;
constexpr size_t kMaxFieldLength = 2048;
// Seven fields with their two length bytes: the most a record read back from the card may be.
constexpr size_t kMaxRecordBytes = 7 * (kMaxFieldLength + 2);

// The key of a book: its Atom id, or the download link of a book that has none.
uint32_t markKey(const std::string& id, const std::string& href);

// False when a URL is too long to store whole (the book is then not marked). A title or an author
// that is too long is cut instead: it only names the file and the progress line.
bool encodeItem(const OpdsQueueItem& item, std::string& out);
// False unless `in` is exactly one record.
bool decodeItem(const std::string& in, OpdsQueueItem& out);
}  // namespace OpdsQueue

// The marked books, in the order they were marked.
class OpdsMarks {
 public:
  size_t size() const { return count; }
  bool empty() const { return count == 0; }
  bool full() const { return count >= OpdsQueue::kMaxMarks; }
  uint32_t at(const size_t index) const { return keys[index]; }
  bool contains(uint32_t key) const;
  // False when the list is full or the key is already there.
  bool add(uint32_t key);
  // False when the key was not there. The order of the rest is kept.
  bool remove(uint32_t key);
  void clear() { count = 0; }

 private:
  uint32_t keys[OpdsQueue::kMaxMarks] = {};
  uint8_t count = 0;
};
