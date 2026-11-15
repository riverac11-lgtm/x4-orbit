# Brief: saved catalog + charge-time delivery (branch orbit-catalog, worktree ~/wt-catalog)
Resume from the WIP commit 1168862f6 (28 files, unfinished, uncompiled; agent was writing "the sync and
notice activities"). Read `git diff 8531d4009 HEAD --stat` then only the files it touches.

BookOrbit = OPDS 1.x behind Cloudflare tunnel (HTTPS, basic auth), a few hundred books, growing. Root
feed rows (photo ui-reference/device-9-opds-root.jpg): All Books, Recent Books, Random Books, Libraries,
Collections, SmartScopes, Authors, Series. Match root entries by title case-insensitively.

1. Saved catalog per server under /.crosspoint/: id, title, author, series+number, format, size, URL,
   date. Streamed, never all in RAM. Full build walks All Books (rel=next); incremental walks Recent Books
   until a known id. Temp file + rename.
2. Online Library opens it instantly: Search, Waiting to download (N), New since last update (N), Series,
   Authors, Browse live server (old browser), Update catalog (last updated). Status per book: On device /
   Waiting. Series view in number order + "Get all missing".
3. Confirm on a book: Get now (Wi-Fi) / Get when charging (persistent waiting list).
4. Charge-time delivery, once per charge session: Wi-Fi on, incremental update, add books of the
   collection "Send to X4" not on device, download waiting list, Wi-Fi off, summary shown once on Home
   (expose a small API for Orbit home layouts). Settings > System: "Download while charging" (default on),
   "Update catalog now". Never Wi-Fi on battery for this.
   MUST work while asleep: wake on USB plug-in if VBUS detect is on a deep-sleep wake GPIO (GPIO0-5);
   ALSO timer wake every ~3 h (setting) -> minimal boot (no display init/refresh), if not charging sleep
   again in < ~300 ms; if charging and due, run the session headless (never touch the panel, 10 min cap,
   abort on unplug or Power press -> normal wake), then deep sleep. Report battery cost estimate and what
   must be tested on the device.
5. "On device" recognises older downloads (download file-name rule) and the waiting list.
Budget ~45 KB flash. Pure logic + host tests in a new suite. Do not push.
