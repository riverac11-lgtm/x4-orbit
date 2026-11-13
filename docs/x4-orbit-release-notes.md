# x4-orbit

Custom firmware for the **Xteink X4 and X3**, built for reading EPUBs and pulling books from a home OPDS library.
Based on [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) 2.41-dev. Personal, unofficial build:
not supported by the Witch(hunt) or CrossPoint projects. Use at your own risk.

## New in this build

- **Simpler home screen**: the book you're reading, **Library**, **Online Library** (your OPDS catalog in one press),
  **More** and **Settings**. Reading stats, bookmarks and file transfer are one step away under More. Each can be put
  back under Settings > Display > Home screen.
- **Sleep screen** shows the cover of the book you're reading with its title and progress (if you hadn't already
  picked a different sleep screen). With no book open it shows the x4-orbit picture.
- **Version in the file name**, so you can tell builds apart on the SD card.
- **Faster page turns on the front buttons**: Left/Right no longer wait 300 ms to check for a double press. (Double
  press to jump 10 pages can be turned back on under Settings > Controls.)
- **Online Library remembers where you were**: it reopens on the shelf and book you left.
- **Download several books at once**: hold **Confirm** on a book to mark it (a check mark appears), then a short
  Confirm on any marked book downloads them all, with progress and a summary at the end. Up to 20 at a time.
- **Better search**: **Left** on the results switches *Whole word* / *Any match*; your last result list is kept
  (reopen Search book to get back to it, **Right** for a new search); **Next match** / **Previous match** in the
  reader menu; your last 8 searches are offered before the keyboard.
- **Speed check** (Settings > System > Speed check): how long book opening, chapter changes, page turns and waking
  take on your reader. **Confirm** saves `speed-report.txt` to the SD card so it can be shared for tuning.

## Still included

Search in book, faster OPDS (one connection per visit), "On device" tags, hold Back to leave the catalog, speed-
compiled reading hot paths, custom boot screen, web server, USB file transfer, SD-card update and recovery mode.
Removed: screenshots, page-as-QR, weather, KOReader sync, Calibre wireless, WebDAV, dictionary, internet updates.

## Install

1. Copy the `v…-orbit.….bin` file below to the SD card, plus a fallback `.bin`
   ([x4-orbit 2.40](https://github.com/riverac11-lgtm/x4-orbit/releases/tag/x4-orbit-2.40) works).
2. On the reader: **Settings > System > SD Card Firmware Update**, pick the file.
3. Open the reader's web page once on Wi-Fi; it fetches the matching `web-….bin` from this release by itself.

To go back, flash the fallback the same way, or hold **Up + Power** while turning the reader on.
**Don't install on** an X4 Pro / X4 Classic, any non-Xteink device, or a USB-locked X4.
