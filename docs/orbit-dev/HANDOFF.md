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
