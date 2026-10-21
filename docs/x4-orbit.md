# x4-orbit: a CrossPoint build for one Xteink X4

A slimmed CrossPoint for an X4 that reads EPUBs and pulls books from an OPDS
server (BookOrbit). Branch: `x4-orbit`. Build profile: `env:x4-orbit`.

## What is different from stock CrossPoint

**Removed** (compiled out with `CP_FEATURE_*` switches in `src/BuildFeatures.h`;
stock profiles are unchanged):
Bluetooth page turner, plugins and Plugin Hub, KOReader sync, dictionary,
clippings, XTC books, internet firmware update, Calibre wireless, WebDAV, LCP
(DRM) downloads, extra themes (Lyra and its cover-grid home remain), X3 support,
UI languages other than English.

**Added**
- **Image look-ahead.** While a page is on screen, images on the next two pages
  are decoded into the SD pixel cache, so turning to them is a cache read. Any
  button press interrupts the work.
- **One TLS connection per OPDS session.** Feeds, next pages, searches and
  covers reuse a kept-alive connection instead of a handshake per request.
- **"On device" marker** on catalog books already in the download folder.
- **Search in book** (reader menu > Search book): case- and accent-insensitive,
  results with chapter and snippet, jumps to the page and underlines the match.

Saved settings, OPDS servers and credentials from stock CrossPoint carry over.

## Build

GitHub: push the `x4-orbit` branch to your fork. The **x4-orbit firmware**
workflow runs the tests and uploads `firmware-x4-orbit.bin` (Actions tab ->
latest run -> Artifacts).

Locally (PlatformIO, with the `freeink-sdk` submodule checked out):

```sh
git submodule update --init --recursive
pio run -e x4-orbit
# firmware: .pio/build/x4-orbit/firmware.bin
```

## Flash

Copy `firmware-x4-orbit.bin` to the SD card and pick it in **Settings > SD Card
Firmware Update**, or connect over USB and run `pio run -e x4-orbit -t upload`. Stock
CrossPoint can be flashed back the same way.
