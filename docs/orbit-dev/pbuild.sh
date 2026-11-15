#!/bin/bash
# Usage: pbuild.sh <worktree dir> <log file>. Builds env check_orbit, one build at a time machine-wide.
cd "$1" || exit 2
exec flock /tmp/claude-0/-home-claude/84f0ce67-e2ce-5e33-9619-ff1995e8944f/scratchpad/pio.lock pio run -e check_orbit > "$2" 2>&1
