#pragma once

#include <cstdint>
#include <string>
#include <vector>

// "Already on the device": the book files in the OPDS download folder, as sorted 32-bit hashes of
// their names without the extension (about 2 KB per 500 books). A catalog book counts as on the
// device when the file name a download of it would get (opdsBookFilename(), any extension) is
// there -- the rule the OPDS browser has always used, so books fetched before the saved catalog
// existed are recognised too.
namespace OpdsOnDevice {

// FNV-1a of the name up to its last '.', ASCII case folded: FAT names compare without case.
uint32_t nameHash(const char* name);
bool isBookFileName(const char* name);
// The download folder, scanned (top level only, as downloads land there).
void scan(std::vector<uint32_t>& sortedHashes);
bool contains(const std::vector<uint32_t>& sortedHashes, const std::string& author, const std::string& title);
// After a download: its path joins the set.
void add(std::vector<uint32_t>& sortedHashes, const std::string& path);
// Where a book is saved: the download folder (created if needed, else the card root) and its name.
std::string downloadPath(const std::string& author, const std::string& title, const char* extension);

}  // namespace OpdsOnDevice
