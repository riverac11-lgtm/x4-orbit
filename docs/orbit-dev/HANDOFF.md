# x4-orbit handoff — RELEASED v2.41-orbit.2 (2026-11-13, x4-orbit = 4baf26e). Read this INSTEAD of re-exploring.

Repo ~/witchhunt (fork riverac11-lgtm/x4-orbit). Published: branch `x4-orbit` = e438b0cd9 (Witch 2.41-dev
+ ours, release `x4-orbit-latest`). Work branch: `orbit-next` (pushed). Fallbacks: `x4-orbit-2.40`,
`x4-orbit-crosspoint`. Never force-push (blocked): fast-forward or `merge -s ours`.

## Sandbox build facts
- Local env `check_orbit` in untracked platformio.local.ini; libdeps pre-seeded (.pio/libdeps/check_orbit,
  SdFat has a fake .piopm). Registry, GraphQL, `gh release`, CI logs are blocked; use `gh api` REST.
- `pio run -e check_orbit` ~2 min incremental, 10+ min clean. Run in background (10 min tool limit).
- Host tests: build/test already configured; full ctest passed 5180/5180 at e438b0cd9.
- CI: push to `x4-orbit` may not trigger; dispatch: `gh api -X POST repos/riverac11-lgtm/x4-orbit/actions/workflows/390901521/dispatches -f ref=x4-orbit`.
- Flash baseline 97.1% (6365283 / 6553600). Keep under 98%.

## Done on orbit-next (COMPILES, 97.1%; HomeMenuTest 17 pass)
- Home entry "Online Library": HomeMenuAction::OnlineLibrary, HomeMenuAvailability.hasOnlineLibrary,
  setting showOnlineLibraryOnHome, ActivityManager::goToOnlineLibrary(), STR_ONLINE_LIBRARY,
  CP_FEATURE_HOME_OPDS.
- src/OrbitProfile.{h,cpp} + CP_ORBIT_PROFILE=1 in [orbit]: one-time simple home (stats/bookmarks/file
  transfer behind More), sleep screen DARK -> COVER + overlay. Hooks: JsonSettingsIO (orbitProfileV),
  CrossPointSettings::loadFromFile.
- src/components/OrbitArt.{h,cpp}: splash art shared by BootActivity and SleepActivity default screen.
- Workflow: versioned asset v<ver>-orbit.<rev>.bin, notes from docs/x4-orbit-release-notes.md.

## Next steps, in order
1. DONE (revision 2, release notes, home tests).
2. DONE.
3. DONE.
4. Agents (briefs: docs/orbit-dev/AGENT_SETUP.md + the three task descriptions below). Worktrees
   ~/wt-opds, ~/wt-search, ~/wt-perf may be gone; recreate from branches orbit-search / orbit-perf
   (WIP commits, unfinished, uncompiled) and orbit-next for OPDS (agent wrote nothing yet).
   - OPDS: remember position; cached feed pages shown instantly; download queue; "new on server"
     shortcut; series text. No cover grid. ~25 KB flash.
   - Search: keep results; next/previous match; whole-word; recent searches; bookmark export. ~20 KB.
   - Perf: "Speed check" screen (book open, chapter change, page turn, wake timings) + /speed-report.txt;
     ahead-of-time indexing only if clearly safe; report on idle poll slice, refresh cadence, wake path. ~15 KB.
5. Merge agent branches into orbit-next, compile, full ctest, clang-format touched files, update
   README + docs/x4-orbit.md, fast-forward `x4-orbit`, dispatch CI, download the versioned .bin from
   releases/download/x4-orbit-latest/, send it to the user.
6. Deferred: parser -O2 (costs ~70 KB; decide after merges), settings "essentials" simplification,
   NAS reading-tracker sync (needs the tracker's API from the user), home/theme visual redesign
   (no simulator; Lyra theme kept).

## User preferences
Non-technical-friendly UI, keep Witch(hunt)'s feel and essential options, speed first, search is
essential. User is usage-limited: work in few, large steps; avoid re-reading; agents are allowed.

## Released in v2.41-orbit.2 (all merged into x4-orbit)
Online Library on home, orbit profile (simple home, cover sleep screen, no 300 ms double-press wait on
Left/Right), orbit art on sleep, search (whole word, kept results in <cache>/search.bin, next/prev match,
recent searches), Speed check (Settings > System > Speed check, /speed-report.txt), OPDS remember position
(/.crosspoint/opds_<hash>_pos.txt) + download queue (hold Confirm to mark). Flash 97.5%.
Next ideas: wait for the user's speed-report.txt; refresh cadence default (15 -> 20-30?); instant cached
OPDS pages; parser -O2 (~70 KB); NAS tracker sync (needs API); Murphy M3 long-Confirm check.
