#pragma once

#include "activities/Activity.h"

/// Settings > System > Speed check.
///
/// What the reader last measured about itself, in milliseconds and plain words: the last book
/// open and where its time went, the last chapter change, and the typical page turn over the last
/// SpeedCheck::kTurnCapacity turns. It exists because the owner of an x4-orbit device has no
/// serial monitor: the figures behind every "it feels slow" used to be log lines nobody could
/// read. Confirm saves the same figures, with the firmware version, the CPU clock and the card, to
/// /speed-report.txt on the SD card, to be sent in.
///
/// One screenful, no scrolling, in the Boot Diagnostics screen's layout: its job is to be read off
/// or photographed. The figures live in RAM only (util/SpeedCheck.h), so they describe the time
/// since the reader was last switched on.
class SpeedCheckActivity final : public Activity {
 public:
  explicit SpeedCheckActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SpeedCheck", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Saved : uint8_t { NotYet, Ok, Failed };
  Saved saved_ = Saved::NotYet;
};
