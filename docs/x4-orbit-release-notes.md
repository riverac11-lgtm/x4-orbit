# x4-orbit v3.0

Custom firmware for the **Xteink X4 and X3**, built for reading EPUBs and pulling books from a home OPDS library
(BookOrbit). Based on [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) 2.41-dev. Personal,
unofficial build: not supported by the Witch(hunt) or CrossPoint projects. Use at your own risk.

From this release the file is called `x4-orbit-v3.0.bin`, and the version shows as **3.0** on the boot screen and in
Settings.

---

## New in v3.0

### 1. Home: "More" is gone, a reading-stats tile takes its place
On Orbit-A, B and C the **More** tile is replaced by a small **reading stats picture**: a ring showing today's reading
against your daily goal, your streak, and seven little bars for the last seven days. Confirm on it opens **Reading
Stats** (today card, month calendar; Left/Right change month).

**File Transfer** moved to **Settings > System > Network > File Transfer**. It works exactly as before.

### 2. Library: Bookmarks tab, no New tab, easier tab switching
- The tabs are now **Books, Recent, Authors, Bookmarks**. *New* is gone; Bookmarks (all your bookmarks across books)
  is its own tab. Confirm on a bookmark jumps into the book at that spot; Left renames it, Right deletes it.
- **Switching tabs works like Settings:** press **Up** on the first row to move onto the tab bar, then **Left/Right**
  to change tab. **Down** or **Confirm** goes back into the list. Holding Up/Down no longer switches tabs.
- **Hold Confirm** on a book for its **Options** (it used to be the Right button). A short press still opens the book.

### 3. Online Library: tidier order, fixes
- The list now starts with **Search**, then **All books**, then Waiting, New, Series, Authors, Browse live server and
  Update catalog.
- **All books** opened with a "Get now" label by mistake; it now opens the A–Z list properly.
- **"Last updated"** was always blank because the reader's clock wasn't set during charging runs. The reader now sets
  its clock over Wi-Fi during an update, and the row shows the date and time of the last update. Until the next
  update runs, it shows **"N books saved"** instead.
- Leftover outlines from the previous screen ("ghosts") are cleared when the Online Library opens.

### 4. More titles per screen
Two-line lists (book title + author) are tighter on the Orbit themes: **12 books per screen instead of 9**. This applies
to the Library, All books, search results, series and authors lists.

### 5. Keyboard: quicker to respond
Keys now act the moment you **press** a button rather than when you let go, which removes about 0.1 s per key. Holds
still work: **hold Confirm** types the key's alternate character (e.g. capital/symbol), **hold Up** moves the text
cursor, **hold Delete** clears the field.

To be honest about it: each key still waits on the e-ink screen redraw (about half a second), and that part is set by
the panel, so typing feels better but not instant.

---

## Already in this build (from v2.41-orbit.3 and .4)

#### 1. Orbit themes (Settings > Display > UI Theme)
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

#### 2. Online Library: your whole catalog, saved on the reader
Online Library now opens a **saved copy of your BookOrbit catalog** instantly, without Wi-Fi.

**The first time**, choose **Save the book list**. On Wi-Fi it walks through *All Books* once, which takes under a
minute for a few hundred books.

**The rows:**
- **Search**: type a few letters of a title, author or series. Results come straight away.
- **All books** *(new in .4)*: every book in the catalog, A to Z by title (a leading "The", "A" or "An"
  is ignored), up to 1,500 books.
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

#### 3. Downloads while charging, even while asleep
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

#### 4. Next in series
When you finish a book and the next one in the series isn't on the reader, the **Book finished** screen offers
**Get next when charging: Title (#n)**, using your saved catalog. It goes on the waiting list. (If the next book is
already on the reader, the existing **Open the next book** row shows instead.)

#### 5. Daily goal and reading calendar (Home > stats tile > Reading Stats)
- **Today card:** a ring filling toward your daily goal ("18 of 30 min"), how much is left, and your streak.
- **Month calendar:** each day is shaded by how much you read, from light grey to black when you met your goal.
  **Left/Right** change the month.
- **Daily goal setting:** Settings > Reader > **Daily Reading Goal** (Off, 10–60 min). It's set to 30 min for you.
- **Orbit-B home** also shows **Read N min today** under the progress bar.
- **No clock set:** the calendar and today card need the clock. Without it the screen shows the usual stats.

#### 6. Battery in days, and chapter marks
- **Days of battery:** the Orbit home screens show **~N days** next to the battery %. It's worked out from how fast
  your battery drains while you read and your usual reading time per day.
  - It appears after the battery has dropped a few percent, and is hidden while charging.
  - It can also go in the reader's status bar: Customise Status Bar > **Days of Reading Left**.
- **Chapter marks:** a thin **book progress bar** is now on at the bottom of the page, with small ticks where
  chapters start.
  - Turn the marks off under Customise Status Bar > **Chapter Marks**; the bar itself is under Lower Progress Bar.

#### 7. Smaller fixes
- **Search keyboard:** now centred.
- **Library folders:** names show without brackets and capitals, so `[COMPLETED]` shows as **Completed**. Nothing
  is renamed on the card.
- **Speed report:** `speed-report.txt` no longer shows up in the Library as a book. It's still in All Files.

---

## Test checklist (v3.0)

| Try this | You should see |
| --- | --- |
| Boot the reader | "3.0" under the orbit picture; Settings shows version 3.0 |
| Home (Orbit-A/B/C) | No More tile; a stats tile with a ring, streak and 7 bars. Confirm opens Reading Stats |
| Settings > System > Network | **File Transfer** is there and starts the web server as before |
| Library | Tabs Books, Recent, Authors, Bookmarks (no New) |
| Library: Up on the first row, then Left/Right | Focus moves onto the tabs; Left/Right change tab; Down/Confirm go back to the list |
| Library: hold Confirm on a book | Options menu opens. A short press opens the book |
| Library > Bookmarks tab | All bookmarks; Confirm opens the book at that place |
| Online Library | Search first, All books second; no stray outlines from the previous screen |
| Online Library > All books | The A–Z list (not "Get now") |
| Online Library > Update catalog (on Wi-Fi), then back | The row shows today's date and time; before that it shows "N books saved" |
| Any two-line book list | 12 books per screen |
| Search keyboard | Keys react on press; hold Confirm for the alternate character, hold Up for the cursor, hold Delete to clear |

**Earlier features to re-check:** downloads while charging, next in series on the Book finished screen, chapter marks
on the bottom bar, and the "~N days" battery estimate.

**Please tell me:**
- Anything that looks crowded with the tighter lists.
- Whether "Last updated" shows a sensible time after a charging run.
- Whether the reader ever wakes or redraws when it shouldn't.

A Speed check report (Settings > System > Speed check > Confirm) after a few days helps too.

---

## Install

1. Copy the `x4-orbit-v….bin` file below to the SD card. Keep your current one as a fallback.
2. On the reader: **Settings > System > SD Card Firmware Update**, then pick the file.
3. Open the reader's web page once on Wi-Fi; it fetches the matching `web-….bin` from this release by itself.

**To go back:** flash the fallback the same way, or hold **Up + Power** while turning the reader on.

**Don't install on:** an X4 Pro / X4 Classic, any non-Xteink device, or a USB-locked X4.
