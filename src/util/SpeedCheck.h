#pragma once

#include <cstddef>
#include <cstdint>

#include "SpeedCheckLog.h"

/// The device half of the Speed check: the one Log instance the reader writes to, with millis()
/// supplied, and the report that Settings > System > Speed check saves to the card.
///
/// The hooks are a store or two each and never touch the card, so they stay in every build.
namespace SpeedCheck {

inline constexpr char kReportPath[] = "/speed-report.txt";
/// Room for the whole report (about 2 KB with a full ring of page turns). Heap, taken only while a
/// report is being written or served; a buffer that is too small still gets whole lines.
inline constexpr size_t kReportBytes = 3072;

Log& log();

// Reader hooks (see SpeedCheckLog.h for what each one means). `eventMs` is the button's release
// edge on the sampler's clock, 0 when the turn had none.
void noteInput(unsigned long eventMs, unsigned long dispatchMs, bool preDrawn);
void sectionEntry(int spine);
void sectionBuilt(int spine, uint16_t pages);
void refreshStart();
void pageVisible();
/// The first page of a book open is on the panel: the phase stamps come from WakeTrace.
void bookOpened(bool fromWake, uint32_t loadMs, uint32_t setupMs, uint32_t sectionMs, uint32_t drawMs,
                uint32_t totalMs, uint32_t pageAtMs);

/// The report as text, with the firmware version, the CPU clock, the power state and the card.
/// `withCardSize` reads the card's free space, which walks its allocation table: seconds on a
/// large card, so only the save asks for it. Returns the bytes written.
size_t buildReport(char* out, size_t cap, bool withCardSize);

/// Writes the report to kReportPath. False when the card or the memory for it could not be had.
bool saveReport();

}  // namespace SpeedCheck
