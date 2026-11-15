#pragma once

#include <functional>
#include <string>
#include <vector>

#include "components/themes/lyra/LyraTheme.h"
#include "components/themes/orbit/OrbitHomeLayout.h"

class GfxRenderer;
struct RecentBook;

// x4-orbit's themes: Lyra's screens with a calmer chrome (OrbitChrome, refinedChrome()) and one of
// three home screens (OrbitHomeLayout). The three are one class with a variant, so they cannot
// differ anywhere but the home screen. Metrics are Lyra's: every list keeps its row heights, and
// with them its paging and touch geometry.
class OrbitTheme final : public LyraTheme {
 public:
  explicit OrbitTheme(OrbitHomeLayout::Variant variant) : variant_(variant) { refinedChrome_ = true; }

  uint8_t orbitHomeVariant() const override { return static_cast<uint8_t>(variant_); }
  bool refinedChrome() const override { return true; }
  const char* submenuMarker() const override { return "\xE2\x80\xBA"; }  // ›
  OrbitHomeLayout::Variant variant() const { return variant_; }

  // The whole home screen below the battery line and above the hints. `books` is HomeActivity's
  // list: the book being read first when `hasCurrent`, then the shelf (C). `selector` is its
  // selector over books, then menu entries. Publishes the tap targets of both, as the Lyra home does.
  void drawOrbitHome(GfxRenderer& renderer, int top, int bottom, const std::vector<RecentBook>& books, bool hasCurrent,
                     bool shelfIsAdded, int selector, int menuCount, const std::function<std::string(int)>& menuLabel,
                     const std::function<UIIcon(int)>& menuIcon) const;

 private:
  OrbitHomeLayout::Variant variant_;
};
