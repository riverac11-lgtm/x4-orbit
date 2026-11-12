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
// Applied once per profile version: to a new settings file, and to an existing one written
// before this version (the file is stamped "orbitProfileV" on save). Only the home-screen layout
// is changed outright; anything else is touched only while it still has its upstream default,
// so a choice the owner already made is left alone.
namespace OrbitProfile {

inline constexpr uint8_t VERSION = 1;

void apply(CrossPointSettings& s);

}  // namespace OrbitProfile
