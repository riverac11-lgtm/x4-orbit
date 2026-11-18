# Brief v3.1-imgui (branch orbit-imgui, worktree ~/wt-imgui). Flash budget: 5 KB.
Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO" items 2 and 4.

A. Large images load by themselves. Today images > 256 KB (LARGE_IMAGE_SOURCE_BYTES) draw as "Large Image /
   Press Confirm to load" (ImageBlock::renderPlaceholder Placeholder::Large; setting largeImagePlaceholder;
   Confirm handler EpubReaderActivity.cpp ~1067 forceLoadLargeImages/pageHasPlaceholders). The reader's
   background warm lane already decodes them with forceLoad (~line 2141 comment).
   Wanted: the box says "Loading image..." (alt text kept if present), the background lane decodes it,
   and when the cache for an image on the page ON SCREEN lands, the page redraws itself once (no button).
   Confirm always opens the reader menu (keep the old force-load path only if needed as a fallback when the
   background lane cannot run, e.g. low memory -> then the existing LowMemory box applies).
   Find whether the warm lane already triggers a redraw of the current page when it finishes an image on it;
   if yes, the job is mostly the label + Confirm. Do not add a redraw loop: at most one redraw per decoded
   page, none if the user turned the page meanwhile. Keep the upstream setting working for other envs;
   for x4-orbit (CP_ORBIT_PROFILE) the new behaviour is the default. Do not regress page-turn speed.
B. Speed check report (src/util/SpeedCheck*, SpeedCheckLog.h, speed-report.txt): add an images section --
   the last N (<=16, bounded ring in RAM or appended lines on SD) image decodes: entry name (short), source
   bytes, format (baseline JPEG / progressive JPEG / PNG / GIF), source WxH, output WxH, decode ms, and
   whether it was a cache hit, background warm or on-the-page-turn decode. Plus totals (count, sum ms).
   Hook where ImageBlock/decoders already log timings. Keep it cheap when Speed check is not running if
   that is how the existing report works.
