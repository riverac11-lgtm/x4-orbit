# x4-orbit handoff — updated 2026-11-15 (stats merged; catalog still running). Read this INSTEAD of re-exploring.

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

## BACKLOG — the user asked to WAIT until he says to implement these
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
