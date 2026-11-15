#include "OpdsOnDevice.h"

#include <HalStorage.h>
#include <Logging.h>
#include <strings.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "OpdsFilename.h"

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

void scan(std::vector<uint32_t>& sortedHashes) {
  sortedHashes.clear();
  const char* folder = SETTINGS.opdsDownloadFolder[0] ? SETTINGS.opdsDownloadFolder : "/";
  auto dir = Storage.open(folder);
  if (!dir || !dir.isDirectory()) return;
  dir.rewindDirectory();
  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(name, sizeof(name));
    if (isBookFileName(name)) sortedHashes.push_back(nameHash(name));
  }
  dir.close();
  std::sort(sortedHashes.begin(), sortedHashes.end());
  LOG_DBG("OPDS", "On-device scan: %u books in %s", static_cast<unsigned>(sortedHashes.size()), folder);
}

bool contains(const std::vector<uint32_t>& sortedHashes, const std::string& author, const std::string& title) {
  if (sortedHashes.empty()) return false;
  // No extension: the hashes leave theirs off, so any format of the book counts.
  const std::string name =
      opdsBookFilename(author, title, static_cast<OpdsFilenameFormat>(SETTINGS.opdsFilenameFormat), "") + ".x";
  return std::binary_search(sortedHashes.begin(), sortedHashes.end(), nameHash(name.c_str()));
}

void add(std::vector<uint32_t>& sortedHashes, const std::string& path) {
  const size_t slash = path.rfind('/');
  const uint32_t h = nameHash(slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1);
  const auto it = std::lower_bound(sortedHashes.begin(), sortedHashes.end(), h);
  if (it == sortedHashes.end() || *it != h) sortedHashes.insert(it, h);
}

std::string downloadPath(const std::string& author, const std::string& title, const char* extension) {
  const char* folder = SETTINGS.opdsDownloadFolder;  // "" => SD root
  bool haveFolder = folder[0] != '\0';
  if (haveFolder && !Storage.exists(folder) && !Storage.mkdir(folder)) {
    LOG_ERR("OPDS", "mkdir failed for %s, using SD root", folder);
    haveFolder = false;
  }
  std::string path;
  path.reserve(96);
  if (haveFolder) path += folder;
  path += '/';
  path += opdsBookFilename(author, title, static_cast<OpdsFilenameFormat>(SETTINGS.opdsFilenameFormat), extension);
  return path;
}

}  // namespace OpdsOnDevice
