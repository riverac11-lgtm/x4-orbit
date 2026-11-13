# x4-orbit agent setup (read fully before starting)

Project: custom e-reader firmware "x4-orbit" for the Xteink X4/X3 (ESP32-C3, 480x800 e-ink, physical
buttons: Back, Confirm, Left, Right, Up, Down, Power; ~380 KB RAM, little free heap, flash 97% full
with ~190 KB spare). It is a fork of Witch(hunt) Reader (C++20, PlatformIO, Arduino-ESP32). Main
checkout: ~/witchhunt, branch `orbit-next`. DO NOT edit ~/witchhunt directly and do not push anything.

## Your own worktree
    git -C ~/witchhunt worktree add ~/wt-<name> -b <branch> orbit-next
    cd ~/wt-<name> && git submodule update --init --recursive      # freeink-sdk, from GitHub
    cp ~/witchhunt/platformio.local.ini .                           # local check env (never commit)
    mkdir -p .pio/libdeps && cp -r ~/witchhunt/.pio/libdeps/check_orbit .pio/libdeps/   # pre-seeded libs
The PlatformIO registry is blocked in this sandbox; the copied libdeps (incl. a seeded SdFat) avoid it.
Never change lib_deps in platformio.ini.

## Build check (required before you finish)
The machine has only 2 cores and other agents build too. ALWAYS build through the shared lock, never
call `pio run` directly:
    /tmp/claude-0/-home-claude/84f0ce67-e2ce-5e33-9619-ff1995e8944f/scratchpad/pbuild.sh ~/wt-<name> ~/wt-<name>/build.log
    grep -E " error|RAM:|Flash:|SUCCESS|FAILED" ~/wt-<name>/build.log
Run it with run_in_background and wait for the completion notification (it may queue behind another
build). Use `cmake --build ... -j1` for host tests for the same reason.
First build takes ~10 min (run it in the background / with a long timeout, do not poll with sleep).
Flash must stay under 98.0% (report the before/after numbers; baseline is 97.1%, 6365283 bytes).
Keep new code small: no new libraries, no large tables, avoid std::function/iostream/heavy templates.

## Host tests
Tests live in test/<suite>/ (CMake + GoogleTest). A full configure+build is slow (>10 min); configure
once and build only the targets you touch:
    cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_SDFAT=/tmp/claude-0/-home-claude/84f0ce67-e2ce-5e33-9619-ff1995e8944f/scratchpad/deps/sdfat
    cmake --build build/test --target <TargetName> -j8 && ctest --test-dir build/test -R <regex>
Add unit tests for any pure logic you add (follow a neighbouring suite's CMakeLists.txt).

## House rules
- Read the surrounding code and follow its conventions (Activity classes, RenderLock, tr(STR_*),
  makeUniqueNoThrow, LOG_DBG/LOG_ERR, HalPowerManager::Lock for CPU-heavy work, comments that explain
  WHY). Heap is scarce: bound every container, prefer SD-card files over RAM, no large stack buffers.
- New UI strings go in lib/I18n/translations/english.yaml only (other languages fall back to English).
  Keys are STR_*; look at how STR_SEARCH_BOOK was added.
- Feature switches live in src/BuildFeatures.h (CP_FEATURE_*, default 1). x4-orbit turns some off in
  the [orbit] section of platformio.ini. Do not remove existing features.
- The audience is a non-technical reader: plain words on screen, obvious button hints, nothing that
  needs a manual. Keep Witch(hunt)'s look (its themes/components: UiListActivity, GUI.draw*, UITheme).
- Run `clang-format -i` (v21 is on PATH) on the C++ files you touched.
- Commit on your branch with clear messages. End each commit message with:
      Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
      Claude-Session: https://claude.ai/code/session_01KxiKr8VCQh6cFFyF972WSo
- No device is available: you cannot run the firmware. Be conservative; reason carefully about
  button handling, memory and SD/Wi-Fi failure paths. If part of the brief turns out risky or does not
  fit in flash, leave it out and say so rather than shipping something doubtful.

## Final report (your last message)
Branch name and worktree path; what you built and how the user reaches it on the device (exact button
path); what you left out and why; flash before/after; tests added and their results; anything the
integrator must know to merge (files touched, new settings keys, strings).
