#pragma once

#include <cstdint>
#include <string>
#include <vector>

// "Already on the device": the book files on the card where downloads and finished books live, as
// sorted 32-bit hashes of their names without the extension (about 2 KB per 500 books). A catalog
// book counts as on the device when the file name a download of it would get (opdsBookFilename(),
// any extension) is there -- the rule the OPDS browser has always used, so books fetched before the
// saved catalog existed are recognised too.
//
// Where: the download folder, each folder directly inside it (one level, for books sorted by hand),
// and /COMPLETED, where a finished book is moved (FinishedBookActivity). Nothing deeper, so a scan
// stays a few directory reads. Every screen and charge run scans afresh when it starts, so a book
// deleted on the reader (file browser, Library, web transfer) is missing again the next time.
namespace OpdsOnDevice {

// Where finished books go; they count as on the device ("Read" in the catalog).
constexpr const char* kCompletedFolder = "/COMPLETED";
// A scan of one place keeps at most this many names (16 KB); the rest are left out.
constexpr size_t kMaxBooks = 4000;

// --- Pure rules (host tested) ---
// FNV-1a of the name up to its last '.', ASCII case folded: FAT names compare without case.
uint32_t nameHash(const char* name);
bool isBookFileName(const char* name);
// Whether a folder found in the download folder is scanned: not hidden (".crosspoint") and not
// /COMPLETED's own name when the download folder is the card root (that one is scanned by itself).
bool scansSubfolder(const char* name, bool inRoot);
bool containsHash(const std::vector<uint32_t>& sortedHashes, uint32_t hash);
// After a download: its path joins the set.
void add(std::vector<uint32_t>& sortedHashes, const std::string& path);

// --- On the card ---
// Every place above into `sortedHashes`; /COMPLETED's books also into `readHashes`, when given.
void scan(std::vector<uint32_t>& sortedHashes, std::vector<uint32_t>* readHashes = nullptr);
// The hash a download of this book would have (its file name, as the settings name it).
uint32_t bookHash(const std::string& author, const std::string& title);
bool contains(const std::vector<uint32_t>& sortedHashes, const std::string& author, const std::string& title);
// Where a book is saved: the download folder (created if needed, else the card root) and its name.
std::string downloadPath(const std::string& author, const std::string& title, const char* extension);

}  // namespace OpdsOnDevice
