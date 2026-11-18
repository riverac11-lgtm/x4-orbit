// The pure half of OpdsOnDevice (no card, no settings), so the host tests can link it.
#include <strings.h>

#include <algorithm>
#include <cstring>

#include "OpdsOnDevice.h"

namespace OpdsOnDevice {

uint32_t nameHash(const char* name) {
  const char* end = strrchr(name, '.');
  if (!end || end == name) end = name + strlen(name);
  uint32_t h = 2166136261u;
  for (const char* p = name; p < end; ++p) {
    const char c = (*p >= 'A' && *p <= 'Z') ? static_cast<char>(*p + 32) : *p;
    h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
  }
  return h;
}

bool isBookFileName(const char* name) {
  const char* dot = strrchr(name, '.');
  if (!dot) return false;
  return strcasecmp(dot, ".epub") == 0 || strcasecmp(dot, ".kepub") == 0 || strcasecmp(dot, ".txt") == 0 ||
         strcasecmp(dot, ".md") == 0 || strcasecmp(dot, ".xtc") == 0 || strcasecmp(dot, ".xtch") == 0;
}

bool scansSubfolder(const char* name, const bool inRoot) {
  if (name[0] == '\0' || name[0] == '.') return false;
  return !(inRoot && strcasecmp(name, kCompletedFolder + 1) == 0);
}

bool containsHash(const std::vector<uint32_t>& sortedHashes, const uint32_t hash) {
  return std::binary_search(sortedHashes.begin(), sortedHashes.end(), hash);
}

void add(std::vector<uint32_t>& sortedHashes, const std::string& path) {
  const size_t slash = path.rfind('/');
  const uint32_t h = nameHash(slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1);
  const auto it = std::lower_bound(sortedHashes.begin(), sortedHashes.end(), h);
  if (it == sortedHashes.end() || *it != h) sortedHashes.insert(it, h);
}

}  // namespace OpdsOnDevice
