#pragma once

#include <cstdint>
#include <string>

#include "components/themes/BaseTheme.h"  // UIIcon
#include "components/themes/orbit/OrbitHomeLayout.h"

class GfxRenderer;

// Paints the Orbit home screens from plain values. Knows nothing of the recent-books store, the
// reading stats or the card: OrbitTheme fills a View from those and hands the covers back through
// `drawCover`. That split is what lets test/orbit_preview render the very same code on the host.
namespace OrbitHomeDraw {

struct Book {
  std::string title;
  std::string author;
  std::string series;
  int percent = -1;     // -1: never opened / unknown
  std::string eta;      // "~1h 03m", or empty
  std::string history;  // "Read 43m 58s over 1 day", or empty
};

// Draws book `index` (0 = the current book, then the shelf) into `slot`: the thumbnail, or a
// no-cover tile. Called for every cover on every render.
using CoverFn = void (*)(void* ctx, GfxRenderer& renderer, int index, const OrbitHomeLayout::Box& slot);

struct View {
  OrbitHomeLayout::Variant variant = OrbitHomeLayout::Variant::A;
  int width = 480;
  int top = 0;     // first row below the battery line
  int bottom = 0;  // last row above the button hints

  bool hasBook = false;
  Book book;
  std::string today;  // B: "Read 18 min today" beside the book's history; empty: nothing read today
  int shelfCount = 0;

  int menuCount = 0;
  const char* menuLabel[OrbitHomeLayout::kMaxMenu] = {};
  UIIcon menuIcon[OrbitHomeLayout::kMaxMenu] = {};
  const uint8_t* menuBitmap[OrbitHomeLayout::kMaxMenu] = {};  // 32 px fallback for icons not drawn here

  int selected = 0;  // HomeActivity's selector: books, then menu entries

  const char* caption = "";     // A: "Continue reading"; C: the greeting
  const char* shelfLabel = "";  // C: "Recently added" / "Recently read"
  const char* noBookTitle = "";
  const char* noBookHint = "";
  const char* leftFormat = "%s";  // "%s left"

  CoverFn drawCover = nullptr;
  void* coverCtx = nullptr;
};

OrbitHomeLayout::Fonts fontsOf(const GfxRenderer& renderer);
OrbitHomeLayout::Frame layout(const GfxRenderer& renderer, const View& view);
void draw(GfxRenderer& renderer, const View& view, const OrbitHomeLayout::Frame& frame);

// Line icons drawn from primitives at any size, centred on (cx, cy): Library (Folder), Online
// Library (Library), More (Ellipsis), Settings, Reading Stats (Stats), Bookmarks (Book). False for
// an icon this does not draw; the caller then uses the bitmap.
bool drawLineIcon(const GfxRenderer& renderer, UIIcon icon, int cx, int cy, int size);

// A thin open chevron, pointing right (or left), centred on (cx, cy), `size` px tall.
void drawChevron(const GfxRenderer& renderer, int cx, int cy, int size, bool right, int thickness = 2);

// The book's progress as a rounded bar: outline, then the read part filled.
void drawProgressBar(const GfxRenderer& renderer, const OrbitHomeLayout::Box& bar, int percent);

}  // namespace OrbitHomeDraw
