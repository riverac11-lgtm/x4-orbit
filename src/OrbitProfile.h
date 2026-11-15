#pragma once

#include <cstdint>

class CrossPointSettings;

// x4-orbit's own first-run choices, laid over Witch(hunt)'s defaults (CP_ORBIT_PROFILE builds only).
//
// The aim is a reader someone can pick up without a manual: the home screen shows the book being
// read, the Library, the Online Library and Settings, and everything else sits one step away
// behind "More". Nothing is removed -- every choice here is an ordinary setting the owner can
// change back under Settings.
//
// Front-button page turns are also made immediate (no double-press wait).
//
// Applied once per profile version: to a new settings file, and to an existing one written
// before this version (the file is stamped "orbitProfileV" on save). Only the home-screen layout
// is changed outright; anything else is touched only while it still has its upstream default,
// so a choice the owner already made is left alone.
namespace OrbitProfile {

// 1: the home menu, page turns, sleep screen.  2: the Orbit-A theme.  3: a 30-minute daily reading
// goal and chapter marks on the progress bar.
inline constexpr uint8_t VERSION = 3;

// Applies the steps a file stamped `fromVersion` has not had yet (0: none, a new card or a stock
// file). A step runs once: a file already on version 1 does not get its home entries reset to
// version 1's choice when version 2 comes along.
void apply(CrossPointSettings& s, uint8_t fromVersion = 0);

}  // namespace OrbitProfile
