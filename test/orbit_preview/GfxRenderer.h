#pragma once

// Host stand-in for lib/GfxRenderer, for test/orbit_preview only: the drawing subset the Orbit home
// and chrome code calls, onto a 1-bit 480x800 portrait raster, with text from the firmware's own
// built-in UI fonts (lib/EpdFont/builtinFonts). It shadows the real header through the include
// order, as test/epub_pipeline's stub does.
//
// Not a reimplementation: the shapes are plain geometry with the device's dither for gray, and text
// advances are summed without the device's differential rounding, so a string may land a pixel off.
// Good enough to judge a layout by eye; not a pixel oracle.

#include <EpdFontFamily.h>
#include <TextTruncation.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

enum Color : uint8_t { Clear = 0x00, White = 0x01, LightGray = 0x05, DarkGray = 0x0A, Black = 0x10 };

class GfxRenderer {
 public:
  static constexpr int W = 480;
  static constexpr int H = 800;
  std::vector<uint8_t> px = std::vector<uint8_t>(W * H, 1);  // 1 white, 0 black
  std::map<int, EpdFontFamily> fonts;

  int getScreenWidth() const { return W; }
  int getScreenHeight() const { return H; }
  void clearScreen() { std::fill(px.begin(), px.end(), 1); }

  void drawPixel(int x, int y, bool state = true) const {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    const_cast<uint8_t&>(px[y * W + x]) = state ? 0 : 1;
  }
  void drawLine(int x1, int y1, int x2, int y2, bool state = true) const {
    const int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    const int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      drawPixel(x1, y1, state);
      if (x1 == x2 && y1 == y2) break;
      const int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x1 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y1 += sy;
      }
    }
  }
  // As the device: extra width goes downward.
  void drawLine(int x1, int y1, int x2, int y2, int lineWidth, bool state) const {
    for (int i = 0; i < lineWidth; i++) drawLine(x1, y1 + i, x2, y2 + i, state);
  }
  void drawRect(int x, int y, int w, int h, bool state = true) const {
    drawLine(x, y, x + w - 1, y, state);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, state);
    drawLine(x + w - 1, y + h - 1, x, y + h - 1, state);
    drawLine(x, y, x, y + h - 1, state);
  }
  void drawRect(int x, int y, int w, int h, int lineWidth, bool state) const {
    for (int i = 0; i < lineWidth; i++) {
      drawLine(x + i, y + i, x + w - i, y + i, state);
      drawLine(x + w - i, y + i, x + w - i, y + h - i, state);
      drawLine(x + w - i, y + h - i, x + i, y + h - i, state);
      drawLine(x + i, y + h - i, x + i, y + i, state);
    }
  }
  void fillRect(int x, int y, int w, int h, bool state = true) const {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) drawPixel(i, j, state);
  }
  void ditherPixel(int x, int y, Color c) const {
    switch (c) {
      case Black:
        drawPixel(x, y, true);
        break;
      case White:
        drawPixel(x, y, false);
        break;
      case LightGray:
        drawPixel(x, y, x % 2 == 0 && y % 2 == 0);
        break;
      case DarkGray:
        drawPixel(x, y, (x + y) % 2 == 0);
        break;
      default:
        break;
    }
  }
  void fillRectDither(int x, int y, int w, int h, Color c) const {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) ditherPixel(i, j, c);
  }
  static bool inRounded(int px_, int py, int x, int y, int w, int h, int r) {
    if (px_ < x || py < y || px_ >= x + w || py >= y + h) return false;
    if (r <= 0) return true;
    const int cx = px_ < x + r ? x + r : (px_ >= x + w - r ? x + w - r - 1 : px_);
    const int cy = py < y + r ? y + r : (py >= y + h - r ? y + h - r - 1 : py);
    const int dx = px_ - cx, dy = py - cy;
    return dx * dx + dy * dy <= r * r;
  }
  void fillRoundedRect(int x, int y, int w, int h, int r, Color c) const {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++)
        if (inRounded(i, j, x, y, w, h, r)) ditherPixel(i, j, c);
  }
  void fillRoundedRect(int x, int y, int w, int h, int r, bool, bool, bool, bool, Color c) const {
    fillRoundedRect(x, y, w, h, r, c);
  }
  void drawRoundedRect(int x, int y, int w, int h, int lw, int r, bool state) const {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++)
        if (inRounded(i, j, x, y, w, h, r) && !inRounded(i, j, x + lw, y + lw, w - 2 * lw, h - 2 * lw, r - lw))
          drawPixel(i, j, state);
  }
  void drawRoundedRect(int x, int y, int w, int h, int lw, int r, bool, bool, bool, bool, bool state) const {
    drawRoundedRect(x, y, w, h, lw, r, state);
  }
  // The device's icons are stored in panel (landscape) orientation; see GfxRenderer::drawIcon.
  void drawIcon(const uint8_t bitmap[], int x, int y, int width, int height) const {
    const int imgW = height, imgH = width, stride = (imgW + 7) / 8;
    for (int j = 0; j < imgH; j++)
      for (int i = 0; i < imgW; i++)
        if (!(bitmap[j * stride + i / 8] & (0x80 >> (i % 8)))) drawPixel(x + width - 1 - j, y + i, true);
  }
  void fillPolygon(const int* xs, const int* ys, int n, bool state = true) const {
    int minY = H, maxY = 0;
    for (int i = 0; i < n; i++) minY = std::min(minY, ys[i]), maxY = std::max(maxY, ys[i]);
    for (int y = minY; y <= maxY; y++) {
      std::vector<int> nodes;
      for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((ys[i] < y && ys[j] >= y) || (ys[j] < y && ys[i] >= y))
          nodes.push_back(xs[i] + (y - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]));
      }
      std::sort(nodes.begin(), nodes.end());
      for (size_t k = 0; k + 1 < nodes.size(); k += 2)
        for (int x = nodes[k]; x <= nodes[k + 1]; x++) drawPixel(x, y, state);
    }
  }

  // ---- Text --------------------------------------------------------------------------------------
  const EpdFontFamily& family(int id) const { return fonts.at(id); }
  const std::map<int, EpdFontFamily>& getFontMap() const { return fonts; }
  int getFontAscenderSize(int fontId) const { return family(fontId).getData()->ascender; }

  // What FreeInkUI's GfxRendererTarget asks of a renderer beyond drawing.
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  Orientation getOrientation() const { return Portrait; }
  void getOrientedViewableTRBL(int* t, int* r, int* b, int* l) const { *t = *r = *b = *l = 0; }
  void drawTextRotated90CW(int, int, int, const char*, bool = true,
                           EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}
  int getLineHeight(int fontId) const { return family(fontId).getData()->advanceY; }
  int getTextHeight(int fontId) const { return family(fontId).getData()->ascender; }
  int getTextWidth(int fontId, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    if (!text) return 0;
    const auto& f = family(fontId);
    int32_t fp = 0;
    uint32_t prev = 0;
    auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
      const uint32_t cp = utf8NextCodepoint(&p);
      if (prev) fp += f.getKerning(prev, cp, style);
      const auto g = f.getGlyph(cp, style);
      if (g) fp += g.advanceX;
      prev = cp;
    }
    return fp4::toPixel(fp);
  }
  void drawText(int fontId, int x, int y, const char* text, bool black = true,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    if (!text) return;
    const auto& f = family(fontId);
    const EpdFontData* d = f.getData(style);
    const int baseline = y + f.getData()->ascender;
    int32_t fp = fp4::fromPixel(x);
    uint32_t prev = 0;
    auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p) {
      const uint32_t cp = utf8NextCodepoint(&p);
      if (prev) fp += f.getKerning(prev, cp, style);
      const auto g = f.getGlyph(cp, style);
      prev = cp;
      if (!g) continue;
      const uint8_t* bits = d->bitmap + epdGlyphBitmapOffset(d, g);
      const int gx = fp4::toPixel(fp) + g.left;
      const int gy = baseline - g.top;
      for (int j = 0; j < g.height; j++)
        for (int i = 0; i < g.width; i++) {
          const int bit = j * g.width + i;
          if (bits[bit / 8] & (0x80 >> (bit % 8))) drawPixel(gx + i, gy + j, black);
        }
      fp += g.advanceX;
    }
  }
  void drawCenteredText(int fontId, int y, const char* text, bool black = true,
                        EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    drawText(fontId, (W - getTextWidth(fontId, text, style)) / 2, y, text, black, style);
  }
  std::string truncatedText(int fontId, const char* text, int maxWidth,
                            EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    if (!text) return {};
    if (getTextWidth(fontId, text, style) <= maxWidth) return text;
    std::string s = text;
    while (!s.empty()) {
      do s.pop_back();
      while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80);
      if (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0xC0) s.pop_back();
      const std::string t = s + textTruncation::ELLIPSIS_UTF8;
      if (getTextWidth(fontId, t.c_str(), style) <= maxWidth) return t;
    }
    return {};
  }
  std::vector<std::string> wrappedText(int fontId, const char* text, int maxWidth, int maxLines,
                                       EpdFontFamily::Style style = EpdFontFamily::REGULAR,
                                       bool* whole = nullptr) const {
    std::vector<std::string> lines;
    std::string rest = text ? text : "";
    if (whole) *whole = true;
    while (!rest.empty() && static_cast<int>(lines.size()) < maxLines) {
      if (getTextWidth(fontId, rest.c_str(), style) <= maxWidth) {
        lines.push_back(rest);
        rest.clear();
        break;
      }
      size_t cut = std::string::npos;
      for (size_t i = 0; i < rest.size(); i++)
        if (rest[i] == ' ' && getTextWidth(fontId, rest.substr(0, i).c_str(), style) <= maxWidth) cut = i;
      if (static_cast<int>(lines.size()) == maxLines - 1 || cut == std::string::npos) {
        lines.push_back(truncatedText(fontId, rest.c_str(), maxWidth, style));
        if (whole) *whole = false;
        rest.clear();
        break;
      }
      lines.push_back(rest.substr(0, cut));
      rest = rest.substr(cut + 1);
    }
    return lines;
  }
};
