# Brief v3.1-lists (branch orbit-lists, worktree ~/wt-lists). Flash budget: 3 KB.
Context: v3.0 is released. Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO" items 1 and 6 only.

A. Page up/down by a full screen with Left/Right in the Online Library lists (OrbitCatalogActivity):
   All books first, and the other long book lists there (search results, series, authors, waiting, new)
   if it is the same code path. One press moves the selection AND the view by exactly the number of rows
   visible (12 on Orbit two-line rows, see OrbitListStyle::kTwoLineRowTrim / UiListActivity
   syncListViewport) -- keep it derived from the viewport, not hard-coded. Clamp at both ends (first/last
   item). Left/Right button hints say so (look for existing STR_LIST_PAGE_PREV / page labels and reuse them).
   Check what Left/Right do today on those screens (UiListActivity may already page) and do not break the
   root menu or the book action menu. If UiListActivity already has a page action, just enable it.

B. "On device" stays accurate (src/util/OpdsOnDevice.*, OrbitCatalog*, ChargeDelivery):
   (a) A book deleted on the reader (file browser delete, Library delete, web File Transfer delete) loses
       its On device mark the next time the catalog is shown -- find how the hash set is cached and
       invalidate/rescan on delete (cheapest correct way).
   (b) Books in /COMPLETED (and any subfolder of the download folder, one level is enough) must count as
       present so nothing re-downloads them: "Get all missing", charge-time "Send to X4" auto-add, the
       waiting list (drop an entry that is already present), and Get now on such a book should not offer a
       duplicate download. Show them as "Read" (new STR_*) instead of "On device" in catalog rows.
       FinishedBookActivity: pathIsInCompleted(), /COMPLETED/<file name>. Note another agent (orbit-done)
       is making /COMPLETED books drop their CACHE; the book FILE stays, so name-based detection works.
   Keep the scan bounded (top level + /COMPLETED), no recursion over the whole card.
   Host tests for any pure logic (extend the existing OpdsOnDevice / catalog suite).
