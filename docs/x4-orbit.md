# x4-orbit: a Witch(hunt) Reader build for the Xteink X4 / X3

A personal build of [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) for reading
EPUBs and pulling books from an OPDS server (BookOrbit). Branch: `x4-orbit`. Build profile:
`env:x4-orbit`.

Witch(hunt) was chosen as the base for its rendering: CSS and embedded-style layout, SD-card
fonts at every size, a 4-level grayscale image pipeline with pixel caches, background
pre-rendering of the next pages and sections, and large images shown as a placeholder that loads
at full quality on Confirm.

## What is different from Witch(hunt)

**Added**
- **Search in book** (reader menu > Navigation > Search book): case- and accent-insensitive,
  searches from the current chapter on and wraps, lists hits with chapter and snippet as they are
  found, jumps to the page and underlines the match. Any button press pauses it; Back stops it and
  keeps the results.
- **One TLS connection per OPDS visit.** Catalog pages, searches, covers and the book download
  reuse one kept-alive connection instead of a handshake each (seconds per request through a
  Cloudflare tunnel).
- **"On device" marker** on catalog books already in the download folder.
- **Long Back leaves the OPDS browser** from any screen in it (lists, loading, errors, book
  details) and goes Home.
- **Own boot splash.**

**Speed**
- **Hot paths built for speed.** The firmware is compiled for size (`-Os`); this profile builds the
  inner loops of opening a book and laying out a chapter -- unzip, the XML tokeniser, text layout
  and hyphenation, glyph lookup and drawing, image conversion -- with `-O2`
  (`scripts/orbit_hot_paths.py`, about 110 KB of flash). A host profile of the chapter pipeline put
  its top costs (line breaking, UTF-8 decoding, the tokeniser, inflate, text measuring) in exactly
  these files.
- **CPU clock.** The ESP32-C3 tops out at 160 MHz; there is no overclock above that. The reader
  already runs at 160 MHz whenever there is work (any button, page builds, image decoding, Wi-Fi,
  search), drops to 10 MHz after half a second idle and light-sleeps after a second.
- **Tracks upstream.** Rebased onto Witch(hunt) 2.41-dev (November 2026) for its measured speed work
  since 2.40: chapters that build while the "Indexing" popup paints, a priority pass for the images
  on the page being shown, faster percent jumps, cheaper glyph and word caches.

**Hidden / compiled out** (`CP_FEATURE_*` switches in `src/BuildFeatures.h`; the upstream envs
are unchanged): screenshots (menu entry and Power+Down), page-as-QR, weather, KOReader sync,
Calibre wireless, WebDAV, dictionary, internet firmware update, the font scaling test screen.
Witch(hunt) has no night-light mode, so there was nothing to remove there.

Kept: the Wi-Fi web server, USB file transfer, SD-card firmware update and the Up+Power recovery
picker, X3 and X4 support in one binary.

## Build

GitHub: push the `x4-orbit` branch. The **x4-orbit firmware** workflow runs the host tests, builds
`env:x4-orbit` and publishes `firmware-x4-orbit.bin` plus its web bundle (`web-<hash>.bin`) to the
rolling pre-release `x4-orbit-latest`.

Locally:

```sh
git submodule update --init --recursive
npm ci
pio run -e x4-orbit
# firmware: .pio/build/x4-orbit/firmware.bin, web bundle: .pio/build/x4-orbit/web-*.bin
```

## Flash

Copy `firmware-x4-orbit.bin` to the SD card and pick it in **Settings > System > SD Card Firmware
Update**. The first time the reader's web page is opened on Wi-Fi, it fetches its web bundle from
the `x4-orbit-latest` release by itself. In hotspot mode, copy the `web-*.bin` from that release
to the top of the SD card instead.

Stock CrossPoint or Witch(hunt) can be flashed back the same way.
