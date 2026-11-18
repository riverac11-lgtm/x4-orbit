# x4-orbit handoff — updated 2026-11-15: backlog 1-11 DONE and released as v3.0. Read this INSTEAD of re-exploring.

Repo ~/witchhunt (fork riverac11-lgtm/x4-orbit). Released: `x4-orbit` = 4baf26e = v2.41-orbit.2
(release x4-orbit-latest). Integration branch: `orbit-next` (pushed). Never force-push; fast-forward
x4-orbit from orbit-next when releasing. Fallback releases: x4-orbit-2.40, x4-orbit-crosspoint.

## Sandbox facts
- Machine has 2 cores. ALWAYS build via docs/orbit-dev/pbuild.sh <worktree> <log> (flock; copy it back to
  the scratchpad path named in AGENT_SETUP.md if missing). Run builds in background. Host tests -j1.
- Local env check_orbit in untracked platformio.local.ini; libdeps pre-seeded in each worktree's .pio.
- Registry/GraphQL/`gh release`/CI logs blocked; use `gh api` REST. CI dispatch:
  gh api -X POST repos/riverac11-lgtm/x4-orbit/actions/workflows/390901521/dispatches -f ref=x4-orbit
- CI publishes v<version>-orbit.<revision>.bin (platformio.ini [orbit] revision, now 2 -> set 3 for the
  next release) + web-*.bin, notes from docs/x4-orbit-release-notes.md.
- Flash: orbit-next at 97.7%. Keep the release <= ~98.6%.

## On orbit-next, NOT yet released (all compiled + tested)
- Keyboard centred in portrait (KeyboardEntryActivity keyboardContentRect).
- Orbit-A/B/C themes (Settings > Display > UI Theme), shared refined chrome (grey pill selection, ›
  chevrons, no « » hints, no empty hint boxes, hairline header, pill tabs), folder names without brackets,
  speed-report.txt hidden from Library/Recent/New, OrbitProfile v2 -> Orbit-A if theme still Lyra.
  Renders: docs/orbit-dev/ui-renders/. Not done: "Read Xm today" (no data; stats branch adds it),
  « » in side hints, FreeInkUI section headers.

## In progress (WIP pushed; resume from briefs, NOT from scratch)
- orbit-catalog (~/wt-catalog): docs/orbit-dev/briefs/catalog.md
- orbit-stats: MERGED into orbit-next (328083ad3) + profile v3 thin book bar. 97.9% flash.
Resume prompt for each agent: "Read ~/witchhunt/docs/orbit-dev/AGENT_SETUP.md and the brief
docs/orbit-dev/briefs/<x>.md; your worktree exists; continue from the WIP commit; read only the WIP diff
and files it touches; commit after each piece that compiles; final concise report."

## After both land
1. Merge orbit-stats and orbit-catalog into orbit-next (english.yaml conflicts: keep both sides).
2. Next in series (#2): on the Book finished screen, look up the next book of the series in the saved
   catalog (offline), offer Download & open (now on Wi-Fi) or Get when charging. Small agent or do inline.
3. Compile, run touched suites, set revision = 3, update docs/x4-orbit-release-notes.md (what's new + HOW
   EACH WORKS + test checklist; the user asked for that explicitly), fast-forward x4-orbit, dispatch CI,
   download the versioned .bin from releases/download/x4-orbit-latest/, send it with the full write-up.

## Feature decisions by the user
Wanted: #2 next in series, #3 charger delivery + saved catalog (his idea), #4 goal+calendar, #6 battery
days+ticks. Not wanted: #1 audiobook catch-up, #5 highlights. He creates a BookOrbit collection
"Send to X4". He wants wake-from-sleep on charger. Usage-limited: few large steps, no re-reading.

## Released: v2.41-orbit.4 (x4-orbit = 6e8a196) — All books A-Z (<=1500) added after .3.

## Released: v3.0 (x4-orbit = orbit-next) — the whole backlog below is DONE (merged orbit-lib, orbit-kbd,
orbit-home; tab-bar hints are bare «/» chevrons). Flash 98.5%. Open idea only: custom fast keyboard waveform.

## v3.1 RELEASED (all 6 to-do items; flash 98.7% = 6467457). Predecode = subset (laid-out chapters of current book only).
## (was: v3.1 IN PROGRESS, user said "implement our list" 2026-11-17)
Agents on branches orbit-lists (items 1,6), orbit-imgui (2,4), orbit-predecode (3), orbit-done (5); worktrees
~/wt-<name>; briefs docs/orbit-dev/briefs/v31-*.md. Flash total must stay <= 98.8% (~19.6 KB over v3.0).
Then: merge --no-ff into orbit-next, combined build+tests, set [orbit] version = 3.1, release notes, push
orbit-next:refs/heads/x4-orbit (push URL https://github.com/riverac11-lgtm/x4-orbit), verify + send bin.

## v3.2 RELEASED (stats rework + clock fix; flash 98.7% = 6471673; 5357 host tests pass). Was: IN PROGRESS (user said "Implement" 2026-11-18): agents orbit-stats (item 1), orbit-clock (item 2);
## worktrees ~/wt-stats, ~/wt-clock; briefs docs/orbit-dev/briefs/v32-*.md. Then merge, build (<=98.8%), tests,
## [orbit] version = 3.2, release notes, push orbit-next:refs/heads/x4-orbit, verify + send bin.
## NEXT TO-DO (v3.2)
1. Reading Stats rework, no day-based stats (the X4 clock is off while asleep, so dates are unreliable;
   photo 2026-11-17: calendar showed 15th on the 17th). Data: ReadingStatsTypes.h (per-book seconds, pages,
   sessions, progress, finishedCount; totals). Layout agreed with user:
   - Ring = current book: % in the middle; "About X h Y min left, roughly N sessions at your pace" (pace from
     that book's seconds/pages or seconds/% ). Under it "Last session: 42 min, 38 pages".
   - "Recent sessions" bars: last 14 sessions (NEW small ring buffer of session length/pages on SD), newest
     right; "Average session · Longest".
   - Milestones: lifetime hours / pages / books finished with the next threshold (10,25,50,100,250,500...;
     pages 1k,2.5k,5k,10k,25k...; books 5,10,15,25,50,100...) and how far to go.
   - Top books stays; merge duplicates by title+author (user saw "The Talisman" twice).
   - Remove: calendar, day streak, "today", Daily Reading Goal setting (OrbitProfile sets it; keep key
     harmless). Orbit home stats tile/dock/row: small book ring + next milestone.
   - "Since last charge" was considered and REJECTED (a charge lasts ~2 weeks).
2. Clock fix (Use Clock stays OFF: user says it costs too much battery). Re-sync time whenever online, not only
   when never set: ChargeDelivery.cpp ~200 uses !HalClock::isSynced() -> use isApproximate()/isStaleRestore()
   (or always, once per Wi-Fi session). Also sync on other Wi-Fi uses (Online Library open/Get now, File
   Transfer) if cheap. Fallback: parse the HTTP "Date:" header of BookOrbit replies (HttpDownloader/OPDS) and
   set the clock when NTP failed and the clock is approximate. Never move the clock backwards in a way that
   breaks anything (stats are no longer day-based after item 1).

## NEXT TO-DO (being implemented as v3.1)
1. Online Library > All books: Left/Right = page up/down, moving the list by one whole screen of items
   (12 rows on Orbit two-line lists). Labels on the Left/Right hints. (Requested 2026-11-15 after v3.0.)
2. Large images: drop "Large Image / Press Confirm to load" (ImageBlock::renderPlaceholder Placeholder::Large,
   >256 KB, setting largeImagePlaceholder; Confirm handler EpubReaderActivity.cpp ~1067). Instead draw a
   "Loading image..." box, decode in the background (existing warm lane already forceLoads), and redraw the
   page by itself when the cache lands. Confirm always opens the reader menu. (2026-11-16)
3. Prepare books while charging: after ChargeDelivery downloads, pre-decode every image (BW + grey caches as the
   reader would) for newly downloaded books AND the book currently being read, so first visits are cache hits.
   Respect the 10-min run cap / unplug abort; resume next charge. (2026-11-16)
4. Speed check report: add per-image timings (entry, bytes, format incl. progressive/baseline/PNG, source WxH,
   decode ms, cache hit/miss) so image speed work targets the real slow case. (2026-11-16)
5. Completed books free their space: when a book lands in /COMPLETED, delete its heavy cache
   (/.crosspoint/epub_<hash of path>/ : laid-out sections, image pixel caches, extracted images, covers?)
   but KEEP finished status, progress, bookmarks, reading stats, and the Library/recents row + a cover thumb.
   Today moveFinishedBookToCompleted (FinishedBookActivity.cpp ~510) calls BookRelocation::bookMoved, which
   MOVES the cache along. Also cover books moved into /COMPLETED by hand (file browser / web transfer) --
   e.g. a sweep at boot or on Library open. Reopening a completed book just rebuilds the cache. (2026-11-16)
6. "On device" stays accurate (2026-11-17). OpdsOnDevice scans ONLY the top level of the download folder by
   file-name hash. Verify + fix: (a) a book deleted on the reader drops its On device mark (rescan / invalidate
   on delete, incl. web-transfer deletes) so it can be fetched again; (b) a book moved to /COMPLETED (or any
   subfolder) must NOT look missing -- otherwise charge runs / Get all missing re-download finished books.
   Show such books as "Read" in the catalog instead of offering them. Also: Send-to-X4 auto-add must skip them.

## BACKLOG (DONE in v3.0, kept for reference)
1. Remove the "More" home entry. Move File Transfer into Settings (System > Network area). Replace the
   More tile/row with a Reading Stats GRAPHIC (e.g. today ring vs goal + streak + mini week bars, from
   ReadingCalendar/StatsDraw) on all three Orbit homes; Confirm opens Reading Stats (deeper view).
   Bookmarks (also behind More today) needs a new home: suggest Library tab or reader menu — ask him.
   Touch points: HomeMenu.cpp entries + HomeMenuTest, OrbitProfile (new VERSION step), OrbitHomeLayout /
   OrbitHomeDraw (a stats tile variant), SettingsList (File Transfer action), test/orbit_preview renders.
2. Keyboard input lag. Finding so far: KeyboardEntryActivity redraws the WHOLE screen and pushes a
   full-panel refresh (renderer.displayBuffer(), line ~979) on every key move. GfxRenderer has
   displayWindow(x,y,w,h). Plan: (a) on cursor moves refresh only the two key cells that changed and on
   typing only the text field (displayWindow, fast mode), full refresh only on layout switch/open;
   (b) coalesce queued presses before rendering (drain input, render once); (c) skip the grey
   anti-alias pass on the keyboard; (d) make sure no 300 ms double-press wait applies on the keyboard's
   buttons; (e) measure with the Speed check page-turn split (button -> refresh start). Expected: from
   a full refresh per key to a small-window fast refresh, the biggest single win.
   Bookmarks answer (user): move Bookmarks into the LIBRARY as a new top tab.
3. Version naming -> our own: v3.0 (then v3.1, ...). Release file x4-orbit-v3.0.bin; Settings header shows
   "3.0" (Witch base 2.41 noted in About/notes). Replace [orbit] revision + workflow naming accordingly;
   firmware CROSSPOINT_VERSION string; web bundle tag stays x4-orbit-latest.
4. Library tabs: remove the "New" tab (overlaps Recent). Keep the library index data it uses, because the
   Orbit-C "Recently added" shelf reads it. Tabs become: Books, Recent, Authors, Bookmarks.
5. Library tab switching like Settings: NO hold Up/Down to change tabs. Up from the first row moves focus
   onto the tab bar; there Left/Right switch tabs; Down returns to the list.
6. Library "Options": no own button/hint. Hold Confirm ("Open") opens the options menu for the selected
   item; short Confirm still opens. Frees that button hint.
7. Online Library root order: Search first, then All books, then the rest (kRowSearch, kRowAll, ...).
8. BUG (cause FOUND): root row kRowAll in OrbitCatalogActivity::provideRow ends with `break;` instead of
   `return;`, so it falls through to the lead-row code below the switch, which sets "Get now". Fix: return.
   (Not a string-table issue.)
9. Tighter spacing in book lists so more titles fit per screen (Online Library lists, Library lists;
   Orbit themes' list row height / padding). Photo ui-reference/device-11-all-books.jpg: All books shows
   only 9 two-line rows (title + author) with ~20 px of padding between rows and empty space at the bottom.
   Target ~12-13 rows: trim row padding, tighter title/author line gap, smaller pill inset; consider an
   option for one-line rows "Title · Author". Keep the selection pill readable; preview with orbit_preview.
   (All books sorting itself looks right on the device: numbers first, then A-Z.)
10. "Last updated: -" never shows a time. Photo device-12: an update HAS run ("New since last update (1)")
   and it still shows "-". So it is not only fullTime vs updateTime: likely the time stamped at sync is 0
   because the clock is not set/trusted during the sync (or the headless session), or localtime_r fails.
   Fix: stamp with the best available time (synced clock; else NTP from the sync's own Wi-Fi session -
   the reader is online then, so fetch time first); show the later of fullTime/updateTime; if still no
   real time, show something useful instead of "-" (e.g. "today", or "N books, updated this session").
11. Ghost marks were visible on the catalog screen photo (old side-hint boxes / hint outlines left on the
   panel): check that the catalog screens do a clean refresh when entering from Home.

## Keyboard lag outcome (orbit-kbd merged)
Per key the cost is ~500 ms of panel FAST refresh (92-95% of the time); windowed refresh was already
measured upstream (docs/future_work/display-and-refresh.md) as saving ~4% with ghosting, so not done.
Shipped: Up/Down/Confirm act on press (saves the 80-150 ms of finger-down time). Remaining big lever:
a faster custom waveform (LUT / direct-update mode) for keyboard screens - needs on-device tuning.
