# Brief v3.2-stats (branch orbit-stats, worktree ~/wt-stats). Flash budget: NET <= +4 KB (removing the
# calendar/goal code should pay for most of it -- measure).
Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO (v3.2)" item 1: it is the agreed design, follow it exactly.
Why: the X4 clock stops while asleep (Use Clock off), so day-based stats are wrong. Nothing on the new
screen may depend on the date.

Code: src/activities/settings/ReadingStatsActivity.cpp, src/components/StatsDraw.{h,cpp},
src/util/ReadingCalendar.*, src/ReadingStats.cpp + ReadingStatsTypes.h (store), the Orbit home stats
tile/dock/row (src/components/themes/orbit/OrbitTheme.cpp, STR_ORBIT_STATS_*), OrbitProfile.cpp (daily goal),
SettingsList.h (Daily Reading Goal), test/orbit_preview (renders) and the stats host tests.

1. Session log: a small bounded ring (last 14-16 sessions: seconds, pages; optional epoch only if synced) on
   SD next to the existing stats files, appended where a reading session ends (find where totalSessions is
   bumped). Tolerate a missing/corrupt file. Sessions under ~1 min are not worth a bar (decide, state it).
2. Screen, top to bottom: current-book ring card (the book last read: % in ring centre; "About X h Y min
   left · ~N sessions at your pace" from that book's seconds per % and the average session length; hide the
   estimate until there is enough data, e.g. >=10 min and >=2% read; "Last session: 42 min, 38 pages");
   Recent sessions bars (newest right, "Average 24 min · Longest 1 h 52 min"); Milestones (hours, pages,
   books finished: value + next threshold + how far to go; thresholds as in HANDOFF); Top books with
   duplicates merged by title+author (case-insensitive), summing their times. Must fit 480x800 portrait in
   the Orbit look (same card style as now), and still work for non-Orbit themes (upstream envs build).
   Left/Right month buttons go away; keep Back and any existing "All books" detail list.
3. Remove the calendar, streak, "today" and the Daily Reading Goal setting from the UI (leave the stored
   settings key harmless so old settings files load). Keep the ReadingStats store's day buckets as they are
   (KOReader/JSON compatibility) -- just stop showing them. Delete ReadingCalendar code that becomes unused.
4. Orbit home stats tile/dock/row (all three themes): small current-book ring + next milestone line.
   Empty state (no reading yet) reads well.
5. Strings in english.yaml; host tests for the pure maths (pace/time-left, milestone thresholds, duplicate
   merge, session ring); regenerate the stats and home renders with OrbitPreview and look at them
   (put PNGs in the scratchpad and name them in your report).
