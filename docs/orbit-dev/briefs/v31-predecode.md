# Brief v3.1-predecode (branch orbit-predecode, worktree ~/wt-predecode). Flash budget: 8 KB.
Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO" item 3, and the charge-time delivery code: src/ChargeDelivery.*,
src/util/ChargeSession.cpp (headless run while charging, asleep included; never touches the panel; 10 min
cap; aborts on unplug or Power press).

Goal: image pages open fast the first time. After the downloads in a charging run, prepare image caches:
first the book currently being read (from the recents / last-open book, starting at the current chapter and
moving forward), then books downloaded in this run. "Prepare" = produce exactly the cache files the reader
will look for later (laid-out section files AND image pixel caches, BW and grey as the reader would use with
the CURRENT reader settings: font, size, margins, orientation, AA). Find the reader's own entry points
(Section building, Section::warmAllImageCaches, Page::warmImageCaches, EpubReaderActivity warm lane) and
reuse them; if cache keys depend on the layout, the section must be built with the same parameters the
reader uses, or the work is wasted -- verify this carefully by reading the code that names the cache files.
Constraints: headless (renderer may exist without the panel being refreshed: check what ChargeSession has);
memory: free Wi-Fi first; per-run time budget inside the 10-min cap (stop cleanly, remember where to resume
next charge in a small state file); abort promptly on unplug/Power; skip books in /COMPLETED. Prefer doing
a bounded amount (e.g. the next 2 chapters of the current book + the first chapter of each new book) if the
full book is too slow -- measure-free reasoning: state your choice and why.
If after reading the code this cannot be done safely headless, implement the safest useful subset (e.g.
only image pixel caches for sections whose layout file already exists) and explain.
Setting: reuse "Download while charging" (no new toggle) unless clearly needed.
