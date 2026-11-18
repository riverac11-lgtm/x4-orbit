#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Pictures prepared while charging (x4-orbit): after a charging session's downloads, the image
// pixel caches of the next pages of the book being read are decoded, so a picture opens from its
// cache the first time it is turned to. Pure rules and the small file they keep, no hardware (host
// tests: test/charge_warm); src/ChargeWarm.cpp runs them.
//
// Why only pages the reader has already laid out: a pixel cache is dithered at the size the
// image takes on the page, and that size comes from the chapter's layout, which depends on the
// font, size, spacing, margins, orientation and the book's own fonts. Laying a chapter out
// without the reader -- no panel, no fonts loaded, no reader settings resolved -- would have to
// reproduce every input of Section::BuildParams exactly, or write a layout the reader never asks
// for (and evict one it does). So the reader writes down, when it closes a book, the layout file
// it used and the frame it drew in; the charging pass reads those same layout files and decodes
// what their pages hold. Its own background build lays out the next chapter while reading, so
// the chapters ahead usually have their layout already.
namespace ChargeWarm {

// Where on the panel the reader draws a page, as it last did: what the decode needs to write
// the same cache the reader's own image lane would (same box, same dither phase).
struct Frame {
  uint16_t panelWidth = 0;  // physical panel, as GfxRenderer::getDisplayWidth()
  uint16_t panelHeight = 0;
  uint16_t panelWidthBytes = 0;
  uint8_t orientation = 0;  // GfxRenderer::Orientation the reader held
  uint8_t grayscale = 0;    // anti-aliasing on: the 4-level cache beside the 1-bit one
  int16_t left = 0;         // the page's left and top margins, in the reader's frame
  int16_t top = 0;
  int16_t viewportHeight = 0;  // for a page of pictures only, centred (ReaderPageLayout)
};

struct State {
  Frame frame;
  uint32_t sectionHash = 0;  // the layout variant: "<spine>_<hash>.bin" in the book's cache
  int16_t spine = 0;         // where the next pass starts
  uint16_t page = 0;
  int16_t endSpine = 0;  // the first spine not prepared
  std::string bookPath;
};

// The chapter the reader is in and this many after it.
constexpr int kSpinesAhead = 2;
constexpr size_t kMaxPathLength = 255;

// What the reader writes when it closes a book at `page` of `spine` (of `spineCount`).
State fromReader(const std::string& bookPath, const Frame& frame, uint32_t sectionHash, int spine, int page,
                 int spineCount);
// The hash in a layout file's name ("/.../12_0a1b2c3d.bin"). False for any other name.
bool hashFromSectionPath(std::string_view path, uint32_t& hash);
// A book in /COMPLETED (finished, kept) is not prepared.
bool inCompleted(std::string_view bookPath);
// A frame the decode can use: a real panel, a known orientation, margins on it.
bool frameUsable(const Frame& frame);
// Something left to prepare for this state.
bool pending(const State& s);
// A chapter finished (or had nothing to prepare): the next one, from its first page.
void nextSpine(State& s);

void encode(const State& s, std::string& out);
bool decode(std::string_view data, State& out);

}  // namespace ChargeWarm
