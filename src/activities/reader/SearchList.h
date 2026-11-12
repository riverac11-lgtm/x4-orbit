#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The last search's result list, kept in a small file in the book's cache folder so it costs no
// heap while the reader is on a page: Search book reopens on it, and the reader menu's
// Next match / Previous match step through it. Also the short list of recent queries.
//
// Hit positions are visible-text offsets (see BookTextSearch.h), which do not depend on the
// layout, so a kept list stays right when the font or margins change.
//
// File layout (little endian, as both the device and the tests are):
//   u32 magic | i16 current | u16 count | u8 flags | u16 skipped | u16 len, query
//   count x { u16 spine, u32 offset, u32 end }
//   count x { u16 len, chapter title (len 0: same as the hit before) | u16 len, snippet }
// The fixed-size table sits before the text so the reader can step to a hit without reading
// any snippet. `current` is rewritten in place when a hit is opened.
//
// The functions are templates over the file so the host tests can run them on a buffer:
// File needs read(void*, size_t), write(const void*, size_t) and seek(size_t).
namespace searchlist {

inline constexpr uint32_t MAGIC = 0x32485253;  // "SRH2"
inline constexpr size_t CURRENT_OFFSET = 4;
inline constexpr size_t MAX_HITS = 80;   // bounds the list's memory on the C3
inline constexpr size_t MAX_TEXT = 512;  // longest query, title or snippet kept
inline constexpr size_t MAX_RECENT = 8;  // recent queries kept
inline constexpr uint8_t FLAG_WHOLE_WORD = 1;
inline constexpr uint8_t FLAG_CAPPED = 2;  // the search stopped at MAX_HITS

inline std::string pathFor(const std::string& bookCacheDir) { return bookCacheDir + "/search.bin"; }
inline constexpr const char* RECENT_PATH = "/.crosspoint/search_recent.txt";

struct Header {
  int16_t current = -1;  // the hit last opened; -1 when none was
  uint16_t count = 0;
  uint8_t flags = 0;
  uint16_t skipped = 0;  // chapters that could not be searched
  std::string query;
};

struct Pos {
  uint16_t spine = 0;
  uint32_t offset = 0;
  uint32_t end = 0;
};

namespace detail {
template <class File, class T>
bool put(File& file, const T& value) {
  return file.write(&value, sizeof(T)) == sizeof(T);
}
template <class File, class T>
bool get(File& file, T& value) {
  return file.read(&value, sizeof(T)) == static_cast<int>(sizeof(T));
}
template <class File>
bool putText(File& file, const std::string& text) {
  // An over-long chapter title is cut, at a character boundary, rather than losing the list.
  size_t size = text.size();
  if (size > MAX_TEXT) {
    size = MAX_TEXT;
    while (size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80) --size;
  }
  const auto len = static_cast<uint16_t>(size);
  return put(file, len) && (len == 0 || file.write(text.data(), len) == len);
}
template <class File>
bool getText(File& file, std::string& text) {
  uint16_t len = 0;
  if (!get(file, len) || len > MAX_TEXT) return false;
  text.resize(len);
  return len == 0 || file.read(text.data(), len) == static_cast<int>(len);
}
}  // namespace detail

template <class File>
bool writeHeader(File& file, const Header& header) {
  return detail::put(file, MAGIC) && detail::put(file, header.current) && detail::put(file, header.count) &&
         detail::put(file, header.flags) && detail::put(file, header.skipped) && detail::putText(file, header.query);
}

template <class File>
bool writePos(File& file, const Pos& pos) {
  return detail::put(file, pos.spine) && detail::put(file, pos.offset) && detail::put(file, pos.end);
}

// One hit's text. An empty title means "the same chapter as the hit before".
template <class File>
bool writeText(File& file, const std::string& title, const std::string& snippet) {
  return detail::putText(file, title) && detail::putText(file, snippet);
}

// Leaves the file at the position table. False for anything that is not a list this build wrote.
template <class File>
bool readHeader(File& file, Header& header) {
  uint32_t magic = 0;
  return detail::get(file, magic) && magic == MAGIC && detail::get(file, header.current) &&
         detail::get(file, header.count) && detail::get(file, header.flags) && detail::get(file, header.skipped) &&
         detail::getText(file, header.query) && header.count <= MAX_HITS && header.current >= -1 &&
         header.current < static_cast<int>(header.count);
}

template <class File>
bool readPos(File& file, Pos& pos) {
  return detail::get(file, pos.spine) && detail::get(file, pos.offset) && detail::get(file, pos.end);
}

template <class File>
bool readText(File& file, std::string& title, std::string& snippet) {
  return detail::getText(file, title) && detail::getText(file, snippet);
}

// Header and position table: all the reader needs to step between hits.
template <class File>
bool readPositions(File& file, Header& header, std::vector<Pos>& hits) {
  if (!readHeader(file, header)) return false;
  hits.resize(header.count);
  for (Pos& pos : hits) {
    if (!readPos(file, pos)) return false;
  }
  return true;
}

template <class File>
bool writeCurrent(File& file, const int16_t current) {
  return file.seek(CURRENT_OFFSET) && detail::put(file, current);
}

// The hit after or before `current`, going round at either end. With none opened yet (-1),
// forward starts at the first hit and backward at the last. -1 for an empty list.
inline int step(const int current, const int count, const bool forward) {
  if (count <= 0) return -1;
  if (forward) return current + 1 >= count ? 0 : current + 1;
  return current <= 0 ? count - 1 : current - 1;
}

// Recent queries, newest first, one per line in the file. False when the list is unchanged
// (the query already heads it), so the caller can skip the write.
inline bool addRecent(std::vector<std::string>& recent, const std::string& query) {
  if (query.empty() || query.find('\n') != std::string::npos) return false;
  if (!recent.empty() && recent.front() == query) return false;
  for (size_t i = 0; i < recent.size(); ++i) {
    if (recent[i] == query) {
      recent.erase(recent.begin() + static_cast<long>(i));
      break;
    }
  }
  recent.insert(recent.begin(), query);
  if (recent.size() > MAX_RECENT) recent.resize(MAX_RECENT);
  return true;
}

inline std::vector<std::string> parseRecent(const std::string& text) {
  std::vector<std::string> recent;
  size_t from = 0;
  while (from < text.size() && recent.size() < MAX_RECENT) {
    size_t to = text.find('\n', from);
    if (to == std::string::npos) to = text.size();
    size_t end = to;
    if (end > from && text[end - 1] == '\r') --end;
    if (end > from) recent.push_back(text.substr(from, end - from));
    from = to + 1;
  }
  return recent;
}

inline std::string joinRecent(const std::vector<std::string>& recent) {
  std::string text;
  for (const auto& query : recent) {
    text += query;
    text += '\n';
  }
  return text;
}

}  // namespace searchlist
