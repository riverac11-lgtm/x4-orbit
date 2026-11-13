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
