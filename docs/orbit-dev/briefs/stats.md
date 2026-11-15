# Brief: battery days, chapter ticks, daily goal + calendar (branch orbit-stats, worktree ~/wt-stats)
Branched from orbit-next c5af3a405 (themes merged). WIP commit 6665d7183 has only 1 file - agent had just
started (was running a baseline build). Mockups: ui-reference/qol_options.png panels 4 and 6.
Use test/orbit_preview to render and look at screens.

A. Battery days: estimate "~N days of reading" from measured drain (small ring of % + time at wake/sleep
   in /.crosspoint/), owner's average daily minutes from stats (else 30). Hide until >= 2% drop observed,
   cap 60 days, nothing while charging. Shown as a selectable status-bar item (default off) and small grey
   text next to battery % on Orbit home screens.
B. Chapter ticks on the reader progress bars (TOC start fractions, max 64, thin if more). Setting
   "Chapter marks", default ON in the orbit profile.
C. Reading Stats: daily goal (Off/10/15/20/30/45/60, orbit default 30 via OrbitProfile VERSION bump using
   apply(s, fromVersion)), today ring, streak, month calendar shaded in 4 greys, Left/Right months.
   Per-day minutes log if none exists (<= 400 days, 2 B/day, written at session end only). Orbit-B shows
   "Read Xm today". No trusted clock -> hide calendar, show streak/today.
Budget ~20 KB flash. Pure logic + host tests. Do not push.
