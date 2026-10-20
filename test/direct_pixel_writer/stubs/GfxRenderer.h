#pragma once

#include <cstdint>

// Minimal renderer for DirectPixelWriter host tests: a full-frame 1-bit
// buffer, no strips, landscape-CCW (identity) orientation.
class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  static constexpr int kWidth = 32;
  static constexpr int kHeight = 8;
  uint8_t frame[kWidth / 8 * kHeight];

  GfxRenderer() { clear(); }
  void clear() {
    for (auto& b : frame) b = 0xFF;  // white
  }

  uint8_t* getWriteTarget() { return frame; }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return kHeight; }
  RenderMode getRenderMode() const { return BW; }
  bool grayPlanesAreAbsolute() const { return false; }
  uint16_t getDisplayWidthBytes() const { return kWidth / 8; }
  int getDisplayWidth() const { return kWidth; }
  int getDisplayHeight() const { return kHeight; }
  Orientation getOrientation() const { return LandscapeCounterClockwise; }
};
