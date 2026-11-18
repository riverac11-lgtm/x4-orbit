#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The pure part of freeing the cache of finished books (src/CompletedBooks): which paths count as
// finished, and the small sorted set of path keys that remembers which books in /COMPLETED were
// already trimmed, so that a sweep over the folder costs a listing and no SD lookup per book.
namespace CompletedLedger {

// A path in /COMPLETED or any folder below it (the finished-book flow and the Library put books
// there; a reader may also move a whole folder in by hand).
inline bool isInCompleted(const std::string_view path) {
  constexpr std::string_view kDir = "/COMPLETED/";
  return path.substr(0, kDir.size()) == kDir;
}

// FNV-1a 32 of the book's path: the same on the device and on a host, unlike std::hash.
inline uint32_t pathKey(const std::string_view path) {
  uint32_t hash = 2166136261u;
  for (const char c : path) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

inline bool contains(const uint32_t* sorted, const size_t count, const uint32_t key) {
  return std::binary_search(sorted, sorted + count, key);
}

// Adds `key` to the sorted set of `count` keys, kept sorted and without repeats. False when the set
// already holds `capacity` keys and `key` is not among them.
inline bool insert(uint32_t* sorted, size_t& count, const size_t capacity, const uint32_t key) {
  uint32_t* const end = sorted + count;
  uint32_t* const at = std::lower_bound(sorted, end, key);
  if (at != end && *at == key) return true;
  if (count >= capacity) return false;
  std::move_backward(at, end, end + 1);
  *at = key;
  ++count;
  return true;
}

}  // namespace CompletedLedger
