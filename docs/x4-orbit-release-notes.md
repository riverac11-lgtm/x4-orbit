# x4-orbit v2.41-orbit.4

Custom firmware for the **Xteink X4 and X3**, built for reading EPUBs and pulling books from a home OPDS library
(BookOrbit). Based on [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) 2.41-dev. Personal,
unofficial build: not supported by the Witch(hunt) or CrossPoint projects. Use at your own risk.

**After updating:** each book lays its pages out once more the first time you open it (the new progress bar at the
bottom takes a few pixels), so the first open of each book is slower.

---

## What's new and how it works

### 1. Orbit themes (Settings > Display > UI Theme)
Three new looks: **Orbit-A**, **Orbit-B** and **Orbit-C**. **Orbit-A** is switched on for you if you were still on the
standard Lyra theme; the old themes are still in the list.

| Theme | Home screen | Moving around |
| --- | --- | --- |
| **Orbit-A** | Your book in a card (cover, % read, time left, progress bar) over four big tiles | Left/Right go through everything; Up/Down move between rows |
| **Orbit-B** | Large cover in the middle, progress under it, a row of icons at the bottom | Left/Right along the row; Up/Down between the cover and the row |
| **Orbit-C** | Greeting, your book, a "Recently added" shelf of 4 covers, then the menu | Up/Down by row; Left/Right across the shelf. Confirm on a cover opens that book |

Every other screen is restyled when an Orbit theme is on:
- Light grey rounded highlight instead of the black bar.
- Proper › arrows instead of `>>`.
- Cleaner tabs and headers.
- Button labels without `«` `»`, and no more empty label boxes.

### 2. Online Library: your whole catalog, saved on the reader
Online Library now opens a **saved copy of your BookOrbit catalog** instantly, without Wi-Fi.

**The first time**, choose **Save the book list**. On Wi-Fi it walks through *All Books* once, which takes under a
minute for a few hundred books.

**The rows:**
- **All books** *(new in .4)*: every book in the catalog, A to Z by title (a leading "The", "A" or "An"
  is ignored), up to 1,500 books.
- **Search**: type a few letters of a title, author or series. Results come straight away.
- **Waiting to download (N)**: books queued for the next charge. Confirm on one takes it off the list; the **Get now**
  row at the top downloads them all now.
- **New since last update (N)**: what was added to BookOrbit at the last update.
- **Series** and **Authors**: browse offline. A series lists its books in order, with **Get all missing** at the top.
- **Browse live server**: the old live catalog browser, for anything else.
- **Update catalog** (shows when it was last updated): fetches only what's new from *Recent Books*, usually one
  request.

Books show **On device** or **Waiting** on the right. Confirm on a book offers:
- **Get now**: downloads straight away on Wi-Fi.
- **Get when charging**: adds it to the waiting list. If it's already waiting, this option is **Don't get it**.

### 3. Downloads while charging, even while asleep
When the reader is on a charger, it updates the saved catalog and downloads everything waiting:
1. Turns Wi-Fi on.
2. Updates the saved catalog.
3. Adds anything in your BookOrbit collection **Send to X4** that isn't on the reader.
4. Downloads the waiting list.
5. Turns Wi-Fi off and goes back to sleep.

**The screen is never touched**: your sleep screen stays as it was. Next time you wake the reader, Home shows what
arrived ("2 new books arrived") with their titles; Confirm fetches anything still waiting.

**When it runs:**
- **Put to sleep while already on the charger:** about 30 seconds later.
- **Plugged in while asleep, X4 with Clock off** (Settings > System > Clock Settings): straight away, because plugging
  in starts the chip.
- **Plugged in while asleep, X4 with the clock on, or an X3:** at the next timed check. The reader wakes for a split
  second, with no screen change, every **3 hours** by default. Change it under **Settings > System > Look for a
  charger while asleep** (Every hour / 3 hours / 6 hours). Without a charger it goes straight back to sleep.
- **Limits:** once per charge, with up to 3 retries if something fails (Wi-Fi, server down). A reader left on its
  charger is checked again every 12 hours. A run stops after 10 minutes.
- **Never on battery.** Unplugging during a run stops it cleanly. Pressing Power during a run stops it and wakes the
  reader normally.

**Settings > System:**
- **Download while charging**: on/off.
- **Look for a charger while asleep**: how often the reader checks.
- **Update catalog now** (under Network): runs an update straight away.

Books downloaded while charging use the cover inside the EPUB.

### 4. Next in series
When you finish a book and the next one in the series isn't on the reader, the **Book finished** screen offers
**Get next when charging: Title (#n)**, using your saved catalog. It goes on the waiting list. (If the next book is
already on the reader, the existing **Open the next book** row shows instead.)

### 5. Daily goal and reading calendar (Home > More > Reading Stats)
- **Today card:** a ring filling toward your daily goal ("18 of 30 min"), how much is left, and your streak.
- **Month calendar:** each day is shaded by how much you read, from light grey to black when you met your goal.
  **Left/Right** change the month.
- **Daily goal setting:** Settings > Reader > **Daily Reading Goal** (Off, 10–60 min). It's set to 30 min for you.
- **Orbit-B home** also shows **Read N min today** under the progress bar.
- **No clock set:** the calendar and today card need the clock. Without it the screen shows the usual stats.

### 6. Battery in days, and chapter marks
- **Days of battery:** the Orbit home screens show **~N days** next to the battery %. It's worked out from how fast
  your battery drains while you read and your usual reading time per day.
  - It appears after the battery has dropped a few percent, and is hidden while charging.
  - It can also go in the reader's status bar: Customise Status Bar > **Days of Reading Left**.
- **Chapter marks:** a thin **book progress bar** is now on at the bottom of the page, with small ticks where
  chapters start.
  - Turn the marks off under Customise Status Bar > **Chapter Marks**; the bar itself is under Lower Progress Bar.

### 7. Smaller fixes
- **Search keyboard:** now centred.
- **Library folders:** names show without brackets and capitals, so `[COMPLETED]` shows as **Completed**. Nothing
  is renamed on the card.
- **Speed report:** `speed-report.txt` no longer shows up in the Library as a book. It's still in All Files.

---

## Test checklist

| Try this | You should see |
| --- | --- |
| Turn the reader on | Orbit-A home with your book card and four tiles; "~N days" appears near the battery after some use |
| Settings > Display > UI Theme > Orbit-B, then Orbit-C | The other two home layouts; Back/Left/Right/Up/Down move as in the table above |
| Online Library (first time) | "Save the book list"; on Wi-Fi it finishes in under a minute |
| Online Library > Search > a few letters | Matching books instantly, with On device / Waiting marks |
| Confirm on a book > Get when charging | It appears under Waiting to download |
| Put a book in your BookOrbit **Send to X4** collection, then put the reader to sleep on the charger | Within about a minute, nothing changes on screen. Wake it later: Home says the books arrived, and they're in the Library |
| Plug in a sleeping reader (not already on the charger) | Downloads happen at the next check (immediately on an X4 with the clock off) |
| Unplug during a charging run | It stops; the next charge picks up the rest |
| Finish a series book whose sequel isn't on the reader | Book finished offers "Get next when charging: …" |
| Read for a few minutes, then More > Reading Stats | Today ring, streak and this month's calendar |
| Any page in a book | Thin bar at the bottom with chapter ticks |
| Search keyboard | Centred on screen |

**Please tell me:**
- Whether the reader ever wakes or redraws when it shouldn't.
- Whether downloads while charging arrive.
- How the battery-days figure compares with real life.

A Speed check report (Settings > System > Speed check > Confirm) after a few days helps too.

---

## Install

1. Copy the `v…-orbit.….bin` file below to the SD card. Keep your current one as a fallback.
2. On the reader: **Settings > System > SD Card Firmware Update**, then pick the file.
3. Open the reader's web page once on Wi-Fi; it fetches the matching `web-….bin` from this release by itself.

**To go back:** flash the fallback the same way, or hold **Up + Power** while turning the reader on.

**Don't install on:** an X4 Pro / X4 Classic, any non-Xteink device, or a USB-locked X4.
