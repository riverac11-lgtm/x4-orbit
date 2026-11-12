#pragma once

// Compile-time feature switches for the x4-orbit build profile (see [orbit] in
// platformio.ini). Every feature defaults to on, so the upstream envs are unaffected; a
// profile turns one off with -DCP_FEATURE_<NAME>=0.

#ifndef CP_FEATURE_SCREENSHOT
#define CP_FEATURE_SCREENSHOT 1  // Reader-menu screenshot and the Power+Down screenshot combo
#endif

#ifndef CP_FEATURE_QR_PAGE
#define CP_FEATURE_QR_PAGE 1  // Reader-menu "show page as QR code"
#endif

#ifndef CP_FEATURE_WEATHER
#define CP_FEATURE_WEATHER 1  // Weather panel (Open-Meteo)
#endif

#ifndef CP_FEATURE_KOSYNC
#define CP_FEATURE_KOSYNC 1  // KOReader progress sync (the position mappers stay: the reader uses them)
#endif

#ifndef CP_FEATURE_CALIBRE
#define CP_FEATURE_CALIBRE 1  // Calibre wireless device connection
#endif

#ifndef CP_FEATURE_WEBDAV
#define CP_FEATURE_WEBDAV 1  // WebDAV share on the file-transfer web server
#endif

#ifndef CP_FEATURE_DICTIONARY
#define CP_FEATURE_DICTIONARY 1  // On-device dictionary lookup
#endif

#ifndef CP_FEATURE_DEV_SCREENS
#define CP_FEATURE_DEV_SCREENS 1  // Developer comparison screens (font scaling test)
#endif

// The x4-orbit look and first-run choices: the orbit picture on the default sleep screen, and a
// one-time move to the simple home screen (see OrbitProfile.h). Off for the upstream envs.
#ifndef CP_ORBIT_PROFILE
#define CP_ORBIT_PROFILE 0
#endif

#ifndef CP_FEATURE_HOME_OPDS
#define CP_FEATURE_HOME_OPDS 1  // "Online Library" (the OPDS catalog) as a home screen entry
#endif

#ifndef CP_FEATURE_OTA
#define CP_FEATURE_OTA 1  // Internet firmware update (the SD-card update stays)
#endif
