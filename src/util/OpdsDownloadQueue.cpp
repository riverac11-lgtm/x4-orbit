#include "OpdsDownloadQueue.h"

#include <utility>

namespace OpdsQueue {
namespace {
void put(std::string& out, const std::string& field, const size_t limit) {
  const size_t length = field.size() < limit ? field.size() : limit;
  out += static_cast<char>(length & 0xFF);
  out += static_cast<char>(length >> 8);
  out.append(field, 0, length);
}

bool take(const std::string& in, size_t& pos, std::string& field) {
  if (in.size() - pos < 2) return false;
  const size_t length = static_cast<uint8_t>(in[pos]) | (static_cast<size_t>(static_cast<uint8_t>(in[pos + 1])) << 8);
  pos += 2;
  if (length > kMaxFieldLength || in.size() - pos < length) return false;
  field.assign(in, pos, length);
  pos += length;
  return true;
}
}  // namespace

uint32_t markKey(const std::string& id, const std::string& href) {
  uint32_t hash = 2166136261u;
  for (const unsigned char c : id.empty() ? href : id) hash = (hash ^ c) * 16777619u;
  return hash;
}

bool encodeItem(const OpdsQueueItem& item, std::string& out) {
  out.clear();
  if (item.url.empty() || item.url.size() > kMaxFieldLength || item.coverUrl.size() > kMaxFieldLength) return false;
  // File names are far shorter than this: OpdsFilename cuts them again.
  constexpr size_t kMaxNameLength = 255;
  put(out, item.title, kMaxNameLength);
  put(out, item.author, kMaxNameLength);
  put(out, item.url, kMaxFieldLength);
  put(out, item.extension, kMaxNameLength);
  put(out, item.mimeType, kMaxNameLength);
  put(out, item.formatKey, kMaxNameLength);
  put(out, item.coverUrl, kMaxFieldLength);
  return true;
}

bool decodeItem(const std::string& in, OpdsQueueItem& out) {
  out = {};
  if (in.size() > kMaxRecordBytes) return false;
  OpdsQueueItem item;
  size_t pos = 0;
  if (!take(in, pos, item.title) || !take(in, pos, item.author) || !take(in, pos, item.url) ||
      !take(in, pos, item.extension) || !take(in, pos, item.mimeType) || !take(in, pos, item.formatKey) ||
      !take(in, pos, item.coverUrl)) {
    return false;
  }
  if (pos != in.size() || item.url.empty()) return false;
  out = std::move(item);
  return true;
}
}  // namespace OpdsQueue

bool OpdsMarks::contains(const uint32_t key) const {
  for (size_t i = 0; i < count; ++i) {
    if (keys[i] == key) return true;
  }
  return false;
}

bool OpdsMarks::add(const uint32_t key) {
  if (full() || contains(key)) return false;
  keys[count++] = key;
  return true;
}

bool OpdsMarks::remove(const uint32_t key) {
  for (size_t i = 0; i < count; ++i) {
    if (keys[i] != key) continue;
    for (size_t j = i + 1; j < count; ++j) keys[j - 1] = keys[j];
    --count;
    return true;
  }
  return false;
}
