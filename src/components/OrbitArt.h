#pragma once

class GfxRenderer;

// The x4-orbit picture: an open book with a tilted orbit around it and a small moon on that orbit
// -- a book on its way in from the library -- under a sparse field of stars, with the name and
// tagline beneath. Drawn from primitives, so there is no bitmap in flash. Shared by the boot
// splash and the default sleep screen.
namespace OrbitArt {

// Draws black-on-white into the current (already cleared) frame, centred, for either orientation.
// Returns the y just below the tagline, where a caller may add its own line.
int draw(const GfxRenderer& r);

}  // namespace OrbitArt
