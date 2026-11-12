#pragma once

// How many pages of layout Background-B keeps ready ahead of the reading position (the page budget
// of EpubReaderActivity::stepBackgroundSectionBuild, see BG_BUILD_LOOKAHEAD_PAGES there).
//
// Upstream's figure is a battery figure: the budget "bounds continuous CPU/SD cost on a battery
// e-reader", and 50 pages is enough for a chapter crossing to be a cache hit when the book is read
// in order. It is not enough for anything else the reader does with a book it is in: skipping a
// chapter ahead, skimming, or jumping from the chapter list all land on a chapter nobody laid out,
// and wait for it.
//
// On external power none of that cost is the battery's. So while a cable is in, the same build
// keeps going for a much longer runway, by the same steps, behind the same gates (the quiet period
// after a page, the heap floors, the image lane first, any button press takes the buffer back and
// drops the build in flight). Nothing new runs: only the point at which B stops asking for the
// next chapter moves. It stays a budget rather than "the rest of the book" so that a web novel of
// a thousand chapters does not fill the card with layouts for chapters months away.
//
// The decision alone, apart from the reader, so the host can test it.
namespace LookaheadBudget {

// Pages of runway on external power. About a long evening's reading; each cached chapter is one
// small file probe when the window is walked again after a chapter change.
inline constexpr int kPoweredPages = 400;

constexpr int pages(const int batteryPages, const bool deepLookahead, const bool onExternalPower) {
  return (deepLookahead && onExternalPower && kPoweredPages > batteryPages) ? kPoweredPages : batteryPages;
}

}  // namespace LookaheadBudget
