#pragma once

#include <string>

// A finished book in /COMPLETED gives back the room its cache folder takes: the laid-out chapters,
// extracted images and pixel caches go (often megabytes per book), and what the reader and the
// Library show of it stays -- finished status, position, bookmarks, metadata, cover thumbnails
// (BookCache::Keep::Finished). Reading stats, recents and the bookmark index live elsewhere and are
// not touched. Reopening the book only takes longer the first time, while it is laid out again.
namespace CompletedBooks {

// Trims the cache of the book at `path`. Never for a book a reader has open: the finished-book flow
// calls it through BookRelocation::bookClosed() once the reader is done with the book. Skipped for a
// book a KOReader sync is about to read. True when the cache is trimmed (or there was none).
bool trim(const std::string& path);

// The Library opened: books put into /COMPLETED by hand (file browser, web File Transfer, a
// computer) are trimmed by the next sweepIfPending(). Due once per boot and again whenever the card
// changed since the last sweep (HalStorage::contentGeneration), so an ordinary Library open costs a
// comparison.
void libraryOpened();

// Runs a sweep libraryOpened() asked for, once. Bounded: a listing of /COMPLETED, a few trims and a
// time budget; a sweep cut short carries on at the next Library open. A small ledger in the cache
// folder remembers the books already trimmed, so they cost no SD lookup.
void sweepIfPending();
bool sweepPending();

}  // namespace CompletedBooks
