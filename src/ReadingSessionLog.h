#pragma once

#include <cstdint>
#include <string>

#include "util/ReadingInsights.h"

// The recent reading sessions (ReadingInsights::SessionLog) in /.crosspoint/reading-sessions.bin,
// beside the reading history. Read whole and written whole: ~160 bytes, once per session end.
// A missing, short or damaged file is an empty log; the next session starts it over.
namespace ReadingSessionLog {

inline constexpr char kPath[] = "/.crosspoint/reading-sessions.bin";

// The log on the card into `out`; false (and an empty log) when there is none to read.
bool load(ReadingInsights::SessionLog& out);
// A finished session of `docId`. Sessions under ReadingInsights::kMinSeconds are left out.
void append(const std::string& docId, uint32_t seconds, uint32_t pages);

}  // namespace ReadingSessionLog
