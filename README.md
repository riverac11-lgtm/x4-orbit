# x4-orbit

A custom e-reader firmware for the **Xteink X4 and X3**, built for one job: reading EPUBs and pulling books straight
from a home OPDS library (BookOrbit). It's based on [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader),
which is itself a fork of [CrossPoint Reader](https://github.com/daveallie/crosspoint-reader).

> Personal, unofficial build. It isn't supported by the Witch(hunt) or CrossPoint projects. Use at your own risk.

**⬇ Download:** [`firmware-x4-orbit.bin` — latest build](https://github.com/riverac11-lgtm/x4-orbit/releases/tag/x4-orbit-latest)

---

## Why Witch(hunt) as the base

Witch(hunt) has the best rendering of the CrossPoint family:

- **Fonts and embedded styles.** It follows the book's own CSS, uses SD-card fonts at every size, and adds large-print sizes.
- **Images.** It renders in 4-level grayscale with an SD pixel cache, so an image is decoded once and redrawn quickly after that.
- **Large images** show as a placeholder at first. Press **Confirm** to load one at full quality.
- **Background pre-rendering** prepares the next pages and chapters while you read, so you see fewer "Indexing" pauses.

## What x4-orbit adds

| | |
| --- | --- |
| 🔍 **Search in book** | Reader menu → *Search book*. Full-text search that ignores case and accents. It starts at your chapter and wraps around, and results appear as they're found, with chapter and snippet. Pick one to jump to that page with the match underlined. Any button pauses the search; Back stops it and keeps the results. |
| ⚡ **Faster OPDS** | One secure connection is kept open for the whole catalog visit. Feeds, next pages, searches, covers and downloads all reuse it instead of paying a TLS handshake each time. That handshake is slow on this chip, especially through a tunnel. |
| ✅ **"On device" tags** | Catalog books already in your download folder are marked, so you don't download them twice. |
| ↩️ **Hold Back to exit** | Holding Back anywhere in the OPDS browser goes straight Home, however deep you've browsed. |
| 🪐 **New boot screen** | An open book, an orbit and a moon on a starfield. It's drawn in code, so it adds no image data to flash. |

## What's removed

The following are compiled out to keep the device lean:

- Screenshots
- Show page as QR code
- Weather
- KOReader sync
- Calibre wireless
- WebDAV
- Dictionary
- Internet firmware updates

**Kept:**

- Wi-Fi web server and USB file transfer
- SD-card firmware updates
- Recovery mode
- X3 and X4 support in a single file

Settings and OPDS servers use the same files as CrossPoint, so they normally carry over.

## Install

1. Copy `firmware-x4-orbit.bin` **and a fallback `.bin`** to the SD card. The fallback can be stock
   [CrossPoint](https://github.com/daveallie/crosspoint-reader/releases) or
   [Witch(hunt)](https://github.com/jpirnay/witchhunt-reader/releases).
2. On the reader, go to **Settings → System → SD Card Firmware Update** and pick the file.
3. Open the reader's web page once while you're on Wi-Fi. It downloads its matching web interface from this
   release automatically. In hotspot mode, copy the `web-….bin` from the release to the top of the SD card instead.
4. If your OPDS login doesn't work after switching, enter it again.

## Going back

- Flash the fallback file the same way.
- If the menus misbehave, hold **Up + Power** while turning the reader on. That opens the SD firmware picker directly.
- If the reader won't boot at all, use the [web installer](https://crosspointreader.com/#flash-tools) over USB-C.

## Don't install it on

| Device | Why |
| --- | --- |
| Xteink X4 Pro, X4 Classic | Different chip (ESP32-S3). The SD updater refuses the file; don't force it with a USB flasher. |
| LilyGo T5S3, reTerminal Sticky, M5PaperMono, any other device | Not built for them; the SD updater refuses the file. |
| **A USB-locked X4** (some third-party-store units) | There's no USB recovery if something goes wrong. Check that the [web installer](https://crosspointreader.com/#flash-tools) sees your device first, or unlock it with the [unlock tool](https://crosspointreader.com/#unlock-tool). |

Also skip it if you need any of the removed features.

