#pragma once

#include <cstdint>

// The Library screen's tabs. The value is what APP_STATE.libraryTab stores, so a value never changes
// meaning: 2 was "New" (the book index's newest books), retired because Recent covers it. Its index
// stays -- Orbit-C's "Recently added" shelf reads it, and Authors builds it.
enum class LibraryTab : uint8_t { Books = 0, Recent = 1, Authors = 3, Bookmarks = 4 };

// The tabs on the bar, in bar order.
constexpr uint8_t LIBRARY_TAB_COUNT = 4;
inline constexpr LibraryTab kLibraryTabOrder[LIBRARY_TAB_COUNT] = {LibraryTab::Books, LibraryTab::Recent,
                                                                   LibraryTab::Authors, LibraryTab::Bookmarks};

// Where `tab` sits on the bar.
constexpr int libraryTabSlot(const LibraryTab tab) {
  for (int slot = 0; slot < LIBRARY_TAB_COUNT; ++slot) {
    if (kLibraryTabOrder[slot] == tab) return slot;
  }
  return 0;
}

// A stored tab. The retired New opens Recent, its nearest neighbour; anything else unknown (a state
// file from another build, or damaged) opens Books.
constexpr LibraryTab libraryTabFrom(const uint8_t stored) {
  if (stored == 2) return LibraryTab::Recent;
  for (const LibraryTab tab : kLibraryTabOrder) {
    if (static_cast<uint8_t>(tab) == stored) return tab;
  }
  return LibraryTab::Books;
}

// The tab `direction` steps from `tab` along the bar, wrapping at both ends.
constexpr LibraryTab libraryTabStep(const LibraryTab tab, const int direction) {
  const int count = LIBRARY_TAB_COUNT;
  return kLibraryTabOrder[((libraryTabSlot(tab) + direction) % count + count) % count];
}

// The focus on the Library's tab bar, as Settings has it: Up from the first row moves it onto the
// bar; there Left/Right change tab and Down or Confirm return to the list.
enum class LibraryBarMove : uint8_t { Stay, PrevTab, NextTab, ToList };

// A short press while the bar holds the focus.
constexpr LibraryBarMove libraryBarMove(const bool left, const bool right, const bool down, const bool confirm) {
  if (left) return LibraryBarMove::PrevTab;
  if (right) return LibraryBarMove::NextTab;
  if (down || confirm) return LibraryBarMove::ToList;
  return LibraryBarMove::Stay;
}

// Whether an Up press from `row` leaves the list for the bar: from the first row that can be
// selected (or before it), instead of wrapping round to the last.
constexpr bool libraryUpToBar(const int row, const int firstSelectable) { return row <= firstSelectable; }
