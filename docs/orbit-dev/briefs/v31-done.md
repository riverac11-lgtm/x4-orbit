# Brief v3.1-done (branch orbit-done, worktree ~/wt-done). Flash budget: 3 KB.
Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO" item 5.

When a book is in /COMPLETED, delete its heavy cache to free SD space: the per-book cache directory
(Epub.cpp ~159: <cacheDir>/epub_<std::hash of path>) contents such as laid-out sections, extracted images,
image pixel caches. KEEP: the book file, finished status, progress, bookmarks, reading stats, the
recents/Library row, and a small cover thumbnail for Library/Home (find which file(s) in the cache dir hold
progress/bookmarks/thumbnail -- they may live in the cache dir! read Epub/Section/BookRelocation/sidecar code
first and keep exactly what is needed so nothing visible changes except a slower first reopen).
Two triggers: (1) moveFinishedBookToCompleted (src/activities/reader/FinishedBookActivity.cpp ~510) after
BookRelocation moves the cache -- careful when the book is still open in the reader (bookMovedWhileOpen):
defer the cleanup until the reader closes it if needed; (2) books moved into /COMPLETED by hand (file
browser move, web File Transfer): a cheap sweep (e.g. once at boot or when the Library opens, bounded,
with a marker so already-cleaned books are not re-scanned each time).
Never delete anything for a book that is currently open. Pure logic (which files to keep) gets host tests.
Another agent (orbit-lists) makes /COMPLETED books show as "Read" in the Online Library; no overlap in files
expected except maybe FinishedBookActivity.
