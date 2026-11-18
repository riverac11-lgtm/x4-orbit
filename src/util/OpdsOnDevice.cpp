#include "OpdsOnDevice.h"

#include <HalStorage.h>
#include <Logging.h>
#include <strings.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "OpdsFilename.h"

namespace OpdsOnDevice {

namespace {
// The books directly in `folder` into `out`; with `subfolders`, those one folder down as well.
void scanFolder(const std::string& folder, const bool subfolders, std::vector<uint32_t>& out) {
  auto dir = Storage.open(folder.c_str());
  if (!dir || !dir.isDirectory()) return;
  dir.rewindDirectory();
  const bool inRoot = folder == "/";
  const bool endsInSlash = !folder.empty() && folder.back() == '/';
  char name[256];
  for (auto file = dir.openNextFile(); file && out.size() < kMaxBooks; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    if (!file.isDirectory()) {
      if (isBookFileName(name)) out.push_back(nameHash(name));
      continue;
    }
    file.close();
    if (subfolders && scansSubfolder(name, inRoot))
      scanFolder(endsInSlash ? folder + name : folder + '/' + name, false, out);
  }
  dir.close();
}
}  // namespace

void scan(std::vector<uint32_t>& sortedHashes, std::vector<uint32_t>* readHashes) {
  sortedHashes.clear();
  const char* folder = SETTINGS.opdsDownloadFolder[0] ? SETTINGS.opdsDownloadFolder : "/";
  scanFolder(folder, true, sortedHashes);
  // /COMPLETED is scanned by itself (unless it is the download folder or directly inside it, and
  // so scanned already): its books are on the device, and they are the read ones.
  std::vector<uint32_t> local;
  std::vector<uint32_t>& read = readHashes ? *readHashes : local;
  read.clear();
  const bool scannedAlready = strcasecmp(folder, kCompletedFolder) == 0;
  if (!scannedAlready || readHashes) scanFolder(kCompletedFolder, false, read);
  std::sort(read.begin(), read.end());
  if (!scannedAlready) sortedHashes.insert(sortedHashes.end(), read.begin(), read.end());
  std::sort(sortedHashes.begin(), sortedHashes.end());
  if (sortedHashes.size() >= kMaxBooks)
    LOG_ERR("OPDS", "On-device scan stopped at %u books", static_cast<unsigned>(kMaxBooks));
  LOG_DBG("OPDS", "On-device scan: %u books (%u read) from %s", static_cast<unsigned>(sortedHashes.size()),
          static_cast<unsigned>(read.size()), folder);
}

uint32_t bookHash(const std::string& author, const std::string& title) {
  // No extension: the hashes leave theirs off, so any format of the book counts.
  const std::string name =
      opdsBookFilename(author, title, static_cast<OpdsFilenameFormat>(SETTINGS.opdsFilenameFormat), "") + ".x";
  return nameHash(name.c_str());
}

bool contains(const std::vector<uint32_t>& sortedHashes, const std::string& author, const std::string& title) {
  return !sortedHashes.empty() && containsHash(sortedHashes, bookHash(author, title));
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
