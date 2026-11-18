# Brief v3.2-clock (branch orbit-clock, worktree ~/wt-clock). Flash budget: 1.5 KB.
Read docs/orbit-dev/HANDOFF.md "NEXT TO-DO (v3.2)" item 2. Use Clock stays OFF for the user; the clock then
stops while asleep and restore() brings back the time saved before sleep, so it falls behind. isSynced() is
true after any restore, so today nothing re-syncs it.
1. Whenever the reader is online and the clock is not NTP-fresh (HalClock::isApproximate() /
   isStaleRestore(); check exact semantics in lib/hal/HalClock.*), sync NTP once per Wi-Fi session: the
   charge run (src/ChargeDelivery.cpp ~200), and the other places Wi-Fi is brought up (find the common
   connect helper; Online Library, Get now, File Transfer, OTA...). Don't add a delay when the clock is
   already fresh; keep the ~5 s worst case off the UI thread where possible or only where a spinner shows.
2. Fallback: the HTTP "Date:" header (RFC 7231 IMF-fixdate, UTC) from BookOrbit/OPDS replies
   (src/network/HttpDownloader.cpp and the OPDS fetch path): when NTP failed or has not run and the clock is
   approximate, set the clock from it (needs a HalClock entry that sets epoch and clears approximate/stale,
   or a "from header" flavour -- keep TLS-plausibility rules intact). Pure parser + host tests.
3. Don't break the TLS "lower bound" logic: a stale clock is still used to validate certs before the sync.
4. Note in docs/x4-orbit.md. No UI changes needed beyond what already shows the time.
