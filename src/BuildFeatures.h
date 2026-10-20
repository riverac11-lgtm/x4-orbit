#pragma once

// Compile-time feature switches for slimmed builds (see [orbit] in
// platformio.ini). Every feature defaults to on, so upstream envs are
// unaffected; a build profile turns one off with -DCP_FEATURE_<NAME>=0.

#ifndef CP_FEATURE_PLUGINS
#define CP_FEATURE_PLUGINS 1  // Plugin catalog, Plugin Hub, plugin event hooks
#endif

#ifndef CP_FEATURE_KOSYNC
#define CP_FEATURE_KOSYNC 1  // KOReader progress sync
#endif

#ifndef CP_FEATURE_DICTIONARY
#define CP_FEATURE_DICTIONARY 1  // On-device dictionary lookup
#endif

#ifndef CP_FEATURE_CLIPPINGS
#define CP_FEATURE_CLIPPINGS 1  // Highlights / clippings
#endif

#ifndef CP_FEATURE_XTC
#define CP_FEATURE_XTC 1  // XTC/XTCH pre-rendered book format
#endif

#ifndef CP_FEATURE_OTA
#define CP_FEATURE_OTA 1  // Internet firmware update (SD-card update stays)
#endif

#ifndef CP_FEATURE_CALIBRE
#define CP_FEATURE_CALIBRE 1  // Calibre wireless device connection
#endif

#ifndef CP_FEATURE_WEBDAV
#define CP_FEATURE_WEBDAV 1  // WebDAV share on the file-transfer web server
#endif

#ifndef CP_FEATURE_EXTRA_THEMES
#define CP_FEATURE_EXTRA_THEMES 1  // Classic, RoundedRaff, Lyra 3 Covers (Lyra always built)
#endif

#ifndef CP_FEATURE_LCP
#define CP_FEATURE_LCP 1  // Readium LCP content protection
#endif
