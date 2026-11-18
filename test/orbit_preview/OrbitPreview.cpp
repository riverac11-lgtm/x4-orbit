// Renders the three Orbit home screens (and the Orbit chrome on a list screen) with fake data into
// 480x800 PGM files, so the layout can be looked at without a device. Run:
//   OrbitPreview <out dir>
// See GfxRenderer.h here for what the stand-in renderer does and does not reproduce.

#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <builtinFonts/inter_ui_10_bold.h>
#include <builtinFonts/inter_ui_10_regular.h>
#include <builtinFonts/inter_ui_12_bold.h>
#include <builtinFonts/inter_ui_12_regular.h>
#include <builtinFonts/notosans_8_regular.h>

#include <cstdio>
#include <string>

#include "GfxRenderer.h"
// After the stand-in renderer: FreeInkUI draws onto it through its GfxRenderer adapter.
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include "components/CardLayout.h"
#include "components/StatsDraw.h"
#include "components/icons/stats.h"
#include "components/themes/orbit/OrbitChrome.h"
#include "components/themes/orbit/OrbitHomeDraw.h"
#include "components/themes/orbit/OrbitListStyle.h"
#include "fontIds.h"
#include "util/ChapterMarks.h"
#include "util/ReadingInsights.h"

namespace {

EpdFont small8(&notosans_8_regular);
EpdFont ui10(&inter_ui_10_regular);
EpdFont ui10b(&inter_ui_10_bold);
EpdFont ui12(&inter_ui_12_regular);
EpdFont ui12b(&inter_ui_12_bold);
EpdFontFamily smallFamily(&small8);
EpdFontFamily ui10Family(&ui10, &ui10b);
EpdFontFamily ui12Family(&ui12, &ui12b);

void writePgm(const GfxRenderer& r, const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    std::perror(path.c_str());
    std::exit(1);
  }
  std::fprintf(f, "P5\n%d %d\n255\n", GfxRenderer::W, GfxRenderer::H);
  for (const uint8_t p : r.px) std::fputc(p ? 255 : 0, f);
  std::fclose(f);
}

// The mock-ups' stand-in for a real cover: dark dither, a hatch, and a label.
void fakeCover(void*, GfxRenderer& r, const int index, const OrbitHomeLayout::Box& s) {
  r.fillRectDither(s.x, s.y, s.w, s.h, Color::DarkGray);
  for (int i = 0; i < s.w + s.h; i += 9) {
    for (int t = 0; t < s.h; t++) {
      const int x = s.x + i - t;
      if (x >= s.x && x < s.x + s.w) r.drawPixel(x, s.y + t, false);
    }
  }
  r.fillRect(s.x + 6, s.y + s.h / 2 - 12, s.w - 12, 24, false);
  const std::string label = index == 0 ? "cover" : "book " + std::to_string(index);
  const int w = r.getTextWidth(SMALL_FONT_ID, label.c_str());
  r.drawText(SMALL_FONT_ID, s.x + (s.w - w) / 2, s.y + s.h / 2 - 9, label.c_str());
  r.drawRect(s.x, s.y, s.w, s.h, true);
}

void battery(GfxRenderer& r) {
  r.drawText(SMALL_FONT_ID, 480 - 12 - 16 - 4 - r.getTextWidth(SMALL_FONT_ID, "90%"), 5, "90%");
  r.drawRect(480 - 12 - 16, 7, 15, 11, true);
  r.fillRect(480 - 12 - 14, 9, 11, 7, true);
  r.fillRect(480 - 12, 10, 2, 5, true);
}

// The Orbit bottom strip: the same parse and box OrbitTheme draws, at the X4's tuned positions.
void hints(GfxRenderer& r, const char* a, const char* b, const char* c, const char* d) {
  const char* labels[4] = {a, b, c, d};
  const int xs[4] = {58, 146, 254, 342};
  for (int i = 0; i < 4; i++) {
    const OrbitChrome::HintLabel label = OrbitChrome::parseHintLabel(labels[i], i == 0);
    if (label.empty()) continue;
    OrbitChrome::drawHintBox(r, xs[i], 800 - 40, 80, 40, label, SMALL_FONT_ID, false);
  }
}

OrbitHomeDraw::View baseView(const OrbitHomeLayout::Variant v, const bool hasBook, const int selected) {
  OrbitHomeDraw::View view;
  view.variant = v;
  view.width = 480;
  view.top = 30;
  view.bottom = 800 - 40 - 8;
  view.hasBook = hasBook;
  view.book = {"Black House", "Stephen King", "The Talisman #2", 41, "~1h 03m", "Read 43m 58s over 1 day"};
  // x4-orbit's home menu: no More; Reading Stats in its place, drawn as the stats graphic.
  static const char* labels[] = {"Library", "Online Library", "Reading Stats", "Settings", "Weather", "Bookmarks"};
  static const UIIcon icons[] = {UIIcon::Folder,   UIIcon::Library, UIIcon::Stats,
                                 UIIcon::Settings, UIIcon::Weather, UIIcon::Book};
  view.menuCount = 4;
  for (int i = 0; i < 6; i++) {
    view.menuLabel[i] = labels[i];
    view.menuIcon[i] = icons[i];
    view.menuBitmap[i] = StatsIcon;
  }
  view.stats.hasHistory = true;
  view.stats.percent = hasBook ? 41 : -1;
  view.stats.heading = "Next milestone";
  view.stats.goal = "50 hours";
  view.stats.toGo = "13 h to go";
  view.stats.compact = "Next: 50 hours";
  view.stats.empty = "Start reading to see your stats";
  view.selected = selected;
  view.caption = v == OrbitHomeLayout::Variant::C ? "Good morning" : "Continue reading";
  view.shelfLabel = "Recently added";
  view.noBookTitle = "No open book";
  view.noBookHint = "Open the Library to start reading";
  view.leftFormat = "%s left";
  view.drawCover = fakeCover;
  return view;
}

void render(const std::string& dir, const std::string& name, OrbitHomeDraw::View view) {
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  battery(r);
  const auto frame = OrbitHomeDraw::layout(r, view);
  OrbitHomeDraw::draw(r, view, frame);
  if (view.variant == OrbitHomeLayout::Variant::C) {
    hints(r, "", "Open", "Up", "Down");
  } else {
    hints(r, "", "Open", "Left", "Right");
  }
  writePgm(r, dir + "/" + name + ".pgm");
}

// Settings > Reader as the device composes it: the theme's header (a title and its rule), the
// FreeInkUI tab bar and list with the tokens UIThemeTokens.h builds, and the hint strip. Lyra's
// metrics (header 84, tab bar 54, padding 20) as OrbitTheme keeps them.
void renderList(const std::string& dir, const std::string& name, const bool orbit, const bool library = false,
                const bool barFocused = false) {
  namespace fui = freeink::ui;
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  battery(r);
  r.drawText(UI_12_FONT_ID, 20, 5 + 40 + 3, library ? "Library" : "Settings", true, EpdFontFamily::BOLD);
  const char* version = "2.41-orbit.3";
  if (!library) r.drawText(SMALL_FONT_ID, 460 - r.getTextWidth(SMALL_FONT_ID, version), 5 + 40 + 10, version);
  if (orbit) {
    r.drawLine(20, 5 + 84 - 2, 459, 5 + 84 - 2, true);
  } else {
    r.drawLine(0, 5 + 84 - 3, 479, 5 + 84 - 3, 3, true);
  }

  fui::GfxRendererTarget target(r);
  target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_10_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_TITLE, UI_12_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_LABEL, SMALL_FONT_ID);
  fui::ThemeTokens tokens = fui::themeTokensForLineHeight(target.lineHeight(fui::GfxRendererTarget::FONT_BODY));
  tokens.headerHeight = 84;
  tokens.footerHeight = 40;
  tokens.listSidePadding = 20;
  tokens.listScrollWidth = 4;
  tokens.listScrollInset = 5;
  if (orbit) OrbitListStyle::applyTokens(tokens, 20);

  static fui::InputSnapshot input;
  fui::InteractionBuffer<64> interactions;
  const fui::DeviceContext device = target.deviceContext();
  fui::Frame<64> frame(target, device, input, interactions);
  fui::Screen<64> screen(frame, tokens);

  fui::TabItem tabs[4] = {{"Display", {}, {}, 0, false},
                          {"Reader", {}, {}, 1, true},
                          {"Controls", {}, {}, 2, false},
                          {"System", {}, {}, 3, false}};
  // The Library's bar (Books, Recent, Authors, Bookmarks), on Authors.
  if (library) {
    tabs[0] = {"Books", {}, {}, 0, false};
    tabs[1] = {"Recent", {}, {}, 1, false};
    tabs[2] = {"Authors", {}, {}, 3, true};
    tabs[3] = {"Bookmarks", {}, {}, 4, false};
  }
  fui::TabBarProps tabProps;
  tabProps.tabs = tabs;
  tabProps.count = 4;
  tabProps.action = 1;
  tabProps.text = tokens.bodyText;
  tabProps.tabInset = fui::Insets{};
  tabProps.contentInset = fui::Insets{};
  if (orbit) {
    OrbitListStyle::applyTabBar(tabProps, barFocused);
  } else {  // ListTabBar's boxed bar, list focused
    fui::StyleSet st;
    st.explicitlySet = true;
    st.normal.background = fui::Paint::solid(fui::Color::White);
    st.normal.border = fui::Paint::solid(fui::Color::Black);
    st.normal.borderWidth = 1;
    st.selected = st.normal;
    st.selected.background = fui::Paint::dither(fui::Color::LightGray);
    st.focused = st.active = st.selected;
    tabProps.tabStyles = st;
  }
  // FileBrowserActivity::composeLibraryTabBar: natural widths, so "Bookmarks" is not cut short.
  if (library) {
    tabProps.layout = fui::TabBarLayout::ContentWidth;
    tabProps.contentInset = fui::Insets{4, 6, 4, 6};
    tabProps.leadingInset = 8;
    tabProps.gap = 4;
  }
  fui::tabBar(frame, fui::Rect{0, 5 + 84, 480 - 40, 54}, tabProps);

  const char* more = orbit ? "\xE2\x80\xBA" : ">>";
  fui::ListItem items[] = {
      {"Reading Orientation", nullptr, "Portrait", {}, {}, fui::StateNormal, 0, true, false},
      {"Reader Font", nullptr, nullptr, {}, {}, fui::StateNormal, 1, true, true},
      {"Reader Font", nullptr, more, {}, {}, fui::StateNormal, 2, true, false},
      {"TXT/MD Font", nullptr, more, {}, {}, fui::StateNormal, 3, true, false},
      {"Font Manager", nullptr, more, {}, {}, fui::StateNormal, 4, true, false},
      {"Publisher Fonts", nullptr, "All Text", {}, {}, fui::StateNormal, 5, true, false},
      {"Layout Settings", nullptr, nullptr, {}, {}, fui::StateNormal, 6, true, true},
      {"Paragraph Alignment", nullptr, "Book's Style", {}, {}, fui::StateNormal, 7, true, false},
      {"Embedded Style", nullptr, nullptr, {}, {}, fui::StateNormal, 8, true, false, true, true},
      {"Hyphenation", nullptr, nullptr, {}, {}, fui::StateNormal, 9, true, false, true, false},
      {"Images", nullptr, more, {}, {}, fui::StateNormal, 10, true, false},
      {"Spacing", nullptr, more, {}, {}, fui::StateNormal, 11, true, false},
  };
  fui::ListProps list;
  list.items = items;
  list.count = sizeof(items) / sizeof(items[0]);
  list.selectedIndex = barFocused ? -1 : 3;
  list.action = 2;
  list.labelText = tokens.bodyText;
  list.labelText.maxLines = 2;
  if (orbit) OrbitListStyle::applyList(list, tokens);
  const fui::ListProps resolved = screen.resolveListProps(list);
  fui::list(frame, fui::Rect{0, 5 + 84 + 54 + 16, 480 - 40, 800 - 40 - 8 - (5 + 84 + 54 + 16)}, resolved);

  if (library && barFocused) {
    hints(r, "\xC2\xAB Home", "Select", "\xC2\xAB Recent", "\xC2\xBB Bookmarks");
  } else if (library) {
    hints(r, "\xC2\xAB Home", "Open", "\xC2\xAB", "\xC2\xBB");
  } else {
    hints(r, "\xC2\xAB Back", "Select", "\xC2\xAB Up", "\xC2\xBB Down");
  }
  writePgm(r, dir + "/" + name + ".pgm");
}

// Reading Stats as ReadingStatsActivity composes it: the Lyra header, the current book, the recent
// sessions, the milestones and the top books, the cards CardLayout draws at the activity's margins
// (Lyra: 32 px), from the same ReadingInsights arithmetic and StatsDraw painting.
void renderStats(const std::string& dir, const std::string& name, const int sessionCount, const uint8_t progress,
                 const uint32_t bookSeconds) {
  using namespace ReadingInsights;
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  battery(r);
  r.drawText(UI_12_FONT_ID, 20, 5 + 40 + 3, "Reading Stats", true, EpdFontFamily::BOLD);
  r.drawLine(20, 5 + 84 - 2, 459, 5 + 84 - 2, true);
  CardLayoutConfig cfg;
  cfg.outerMarginX = 32;
  CardLayout layout(r, Rect{0, 0, 480, 760}, 5 + 84 + 16, cfg);
  const auto dur = [](char* out, const size_t size, const uint32_t seconds) {
    formatDuration(out, size, seconds, "%d h %d min", "%d h", "%d min");
  };

  SessionLog log;
  const uint32_t minutes[] = {24, 41, 12, 65, 30, 18, 112, 22, 35, 9, 48, 27, 15, 42, 33, 20};
  for (int i = 0; i < sessionCount; ++i) add(log, "0123456789abcdef0123456789abcdef", minutes[i] * 60, minutes[i]);

  char line[48] = "", note[48] = "", last[64] = "", text[24];
  const TimeLeft left = timeLeft(bookSeconds, progress, averageSeconds(log));
  if (progress >= 100) {
    snprintf(line, sizeof(line), "Finished");
  } else if (left.known) {
    dur(text, sizeof(text), left.seconds);
    snprintf(line, sizeof(line), "About %s left", text);
    if (left.sessions > 1) snprintf(note, sizeof(note), "~%d sessions at your pace", left.sessions);
  } else {
    snprintf(note, sizeof(note), "Keep reading to see time left");
  }
  if (const Session* n = newest(log)) {
    dur(text, sizeof(text), n->seconds);
    snprintf(last, sizeof(last), "Last session: %s, %d pages", text, static_cast<int>(n->pages));
  }
  StatsDraw::Book book;
  book.percent = progress;
  book.title = "Black House";
  book.line = line;
  book.note = note;
  book.last = last;
  layout.card(nullptr, [&](CardLayout::Body& b) {
    b.advance(StatsDraw::drawBook(r, b.innerLeft(), b.currentY(), b.innerWidth(), book));
  });

  const int smallH = r.getLineHeight(SMALL_FONT_ID);
  layout.card("Recent sessions", [&](CardLayout::Body& b) {
    if (log.count == 0) {
      b.centeredMessage("Your reading sessions will show here");
      return;
    }
    uint32_t seconds[kCapacity];
    for (int i = 0; i < log.count; ++i) seconds[i] = log.sessions[i].seconds;
    b.advance(
        StatsDraw::drawSessionBars(r, b.innerLeft(), b.currentY(), b.innerWidth(), 48, seconds, log.count, kCapacity) +
        4);
    char average[24], longest[24], caption[64];
    dur(average, sizeof(average), averageSeconds(log));
    dur(longest, sizeof(longest), longestSeconds(log));
    snprintf(caption, sizeof(caption), "Average %s \xC2\xB7 Longest %s", average, longest);
    r.drawText(SMALL_FONT_ID, b.innerLeft(), b.currentY(), caption);
    b.advance(smallH);
  });

  layout.card("Milestones", [&](CardLayout::Body& b) {
    const struct {
      Kind kind;
      uint32_t amount;
      const char* label;
      const char* goal;
    } rows[] = {{Kind::Hours, 37 * 3600 + 20 * 60, "Hours read", "%s hours"},
                {Kind::Pages, 4812, "Pages turned", "%s pages"},
                {Kind::Books, 3, "Books finished", "%s books"}};
    for (int i = 0; i < 3; ++i) {
      const Milestone m = milestone(rows[i].kind, rows[i].amount);
      char value[16], threshold[16], goal[32], toGo[24], next[72];
      groupThousands(value, sizeof(value), m.value);
      groupThousands(threshold, sizeof(threshold), m.next);
      snprintf(goal, sizeof(goal), rows[i].goal, threshold);
      if (m.kind == Kind::Hours) {
        dur(toGo, sizeof(toGo), m.toGo);
      } else {
        groupThousands(toGo, sizeof(toGo), m.toGo);
      }
      snprintf(next, sizeof(next), "Next: %s \xC2\xB7 %s to go", goal, toGo);
      StatsDraw::Milestone view{rows[i].label, value, next, m.permille};
      if (i > 0) b.advance(8);
      b.advance(StatsDraw::drawMilestone(r, b.innerLeft(), b.currentY(), b.innerWidth(), view));
    }
  });

  layout.card("Top books", [&](CardLayout::Body& b) {
    const char* titles[] = {"The Talisman", "Black House", "Middlemarch"};
    const char* times[] = {"9h 12m", "6h 40m", "3h 05m"};
    for (int i = 0; i < 3; ++i) {
      const int w = r.getTextWidth(UI_10_FONT_ID, times[i]);
      r.drawText(UI_10_FONT_ID, b.innerRight() - w, b.currentY(), times[i]);
      r.drawText(UI_10_FONT_ID, b.innerLeft(), b.currentY(), titles[i], true, EpdFontFamily::BOLD);
      b.advance(b.rowStep());
    }
  });
  if (layout.cursorY() > 760 - 16)
    std::fprintf(stderr, "%s: cards run past the hints (%d)\n", name.c_str(), layout.cursorY());
  hints(r, "\xC2\xAB Back", "All books", "", "");
  writePgm(r, dir + "/" + name + ".pgm");
}

// The reader's bottom edge as BaseTheme::drawStatusBar draws it with a book progress bar (medium, 4 px,
// under the bezel) and chapter marks, items above it: the battery, "~9 days", the chapter, the page.
void renderReaderBar(const std::string& dir, const std::string& name, const int barHeight, const bool rtl) {
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  for (int y = 60; y < 700; y += 28) r.fillRectDither(40, y, (y / 28) % 5 == 4 ? 260 : 400, 10, Color::LightGray);
  ChapterMarks marks;
  constexpr int kChapters[] = {6, 11, 19, 27, 31, 40, 46, 53, 61, 66, 72, 81, 89, 94};
  for (const int pct : kChapters) marks.add(static_cast<uint16_t>(pct * 65535 / 100));
  const int y = 800 - barHeight;
  const int filled = 480 * 41 / 100;
  r.fillRect(rtl ? 480 - filled : 0, y, filled, barHeight, true);
  StatsDraw::drawChapterTicks(r, 0, y, 480, barHeight, filled, rtl, /*upward=*/true, 2, marks);
  // UITheme::getStatusBarItemsHeight(): statusBarVerticalMargin 19 + 2 * 4 padding + 4 clearance.
  const int textY = y - 1 - 31 + 4;
  r.drawRect(20, textY + 6, 15, 11, true);
  r.fillRect(22, textY + 8, 9, 7, true);
  r.drawText(SMALL_FONT_ID, 20 + 15 + 4, textY, "90%");
  const int bx = 20 + 15 + 4 + r.getTextWidth(SMALL_FONT_ID, "90%") + 1 + 6;
  r.drawText(SMALL_FONT_ID, bx, textY, "~9 days");
  const char* page = "4/12  41%";
  r.drawText(SMALL_FONT_ID, 460 - r.getTextWidth(SMALL_FONT_ID, page), textY, page);
  const char* chapter = "Chapter 19";
  r.drawText(SMALL_FONT_ID, (480 - r.getTextWidth(SMALL_FONT_ID, chapter)) / 2, textY, chapter);
  writePgm(r, dir + "/" + name + ".pgm");
}

// A two-line book list (title + author) as the Online Library's All books draws it: the theme's
// header, then UiListActivity::addList()'s explicit row height (ThemeMetrics::
// listWithSubtitleRowHeight) on a non-touch panel. `rowHeight` 68 is Lyra's; Orbit takes
// OrbitListStyle::kTwoLineRowTrim off it. Prints how many rows fit.
void renderBookList(const std::string& dir, const std::string& name, const int rowHeight) {
  namespace fui = freeink::ui;
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  battery(r);
  r.drawText(UI_12_FONT_ID, 20, 5 + 40 + 3, "All books", true, EpdFontFamily::BOLD);
  r.drawLine(20, 5 + 84 - 2, 459, 5 + 84 - 2, true);

  fui::GfxRendererTarget target(r);
  target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_10_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_TITLE, UI_12_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_LABEL, SMALL_FONT_ID);
  fui::ThemeTokens tokens = fui::themeTokensForLineHeight(target.lineHeight(fui::GfxRendererTarget::FONT_BODY));
  tokens.headerHeight = 84;
  tokens.footerHeight = 40;
  tokens.listSidePadding = 20;
  tokens.listScrollWidth = 4;
  tokens.listScrollInset = 5;
  OrbitListStyle::applyTokens(tokens, 20);

  static fui::InputSnapshot input;
  fui::InteractionBuffer<64> interactions;
  const fui::DeviceContext device = target.deviceContext();
  fui::Frame<64> frame(target, device, input, interactions);
  fui::Screen<64> screen(frame, tokens);

  static const char* const kBooks[][2] = {
      {"14", "Peter Clines"},
      {"20th Century Ghosts", "Joe Hill"},
      {"Accomplice to the Villain", "Hannah Nicole Maehrer"},
      {"Adversary to the Villain", "Hannah Nicole Maehrer"},
      {"Alchemised", "SenLinYu"},
      {"All Systems Red: The Murderbot Diaries", "Martha Wells"},
      {"American Gods", "Neil Gaiman"},
      {"Apprentice to the Villain", "Hannah Nicole Maehrer"},
      {"Arcanum Unbounded", "Brandon Sanderson"},
      {"Artificial Condition", "Martha Wells"},
      {"The Blade Itself", "Joe Abercrombie"},
      {"Children of Time", "Adrian Tchaikovsky"},
      {"The Way of Kings", "Brandon Sanderson"},
      {"Words of Radiance", "Brandon Sanderson"},
      {"Project Hail Mary", "Andy Weir"},
  };
  constexpr int kCount = sizeof(kBooks) / sizeof(kBooks[0]);
  fui::ListItem items[kCount];
  for (int i = 0; i < kCount; ++i) {
    items[i].label = kBooks[i][0];
    items[i].subtitle = kBooks[i][1];
    items[i].actionValue = static_cast<int16_t>(i);
  }
  items[4].value = "On device";
  fui::ListProps list;
  list.items = items;
  list.count = kCount;
  list.selectedIndex = 0;
  list.action = 2;
  list.labelText = tokens.bodyText;
  list.labelText.maxLines = 2;
  list.rowHeight = static_cast<int16_t>(rowHeight);
  OrbitListStyle::applyList(list, tokens);
  const fui::ListProps resolved = screen.resolveListProps(list);
  // UiListActivity::layoutListArea(): below the header and its verticalSpacing, above the hints.
  const fui::Rect area{0, 5 + 84 + 16, 480 - 20, 800 - 40 - 8 - (5 + 84 + 16)};
  fui::list(frame, area, resolved);
  std::printf("%s: row %d px, body %d px -> %d rows (label line %d, subtitle line %d)\n", name.c_str(), rowHeight,
              area.height, area.height / rowHeight, target.lineHeight(resolved.labelText.font),
              target.lineHeight(resolved.subtitleText.font));
  hints(r, "\xC2\xAB Back", "Select", "\xC2\xAB Up", "\xC2\xBB Down");
  writePgm(r, dir + "/" + name + ".pgm");
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  using OrbitHomeLayout::Variant;
  render(dir, "home-a", baseView(Variant::A, true, 2));
  render(dir, "home-a-card", baseView(Variant::A, true, 0));
  auto six = baseView(Variant::A, true, 6);
  six.menuCount = 6;
  render(dir, "home-a-six", six);
  render(dir, "home-a-nobook", baseView(Variant::A, false, 1));
  render(dir, "home-b", baseView(Variant::B, true, 1));
  render(dir, "home-b-cover", baseView(Variant::B, true, 0));
  render(dir, "home-b-nobook", baseView(Variant::B, false, 0));
  auto c = baseView(Variant::C, true, 6);
  c.shelfCount = 4;
  render(dir, "home-c", c);
  auto cShelf = baseView(Variant::C, true, 2);
  cShelf.shelfCount = 4;
  render(dir, "home-c-shelf", cShelf);
  auto cNoBook = baseView(Variant::C, false, 3);
  cNoBook.shelfCount = 2;
  render(dir, "home-c-nobook", cNoBook);
  renderList(dir, "list-orbit", true);
  renderBookList(dir, "list-books-before", 68);
  renderBookList(dir, "list-books-after", 68 - OrbitListStyle::kTwoLineRowTrim);
  renderList(dir, "library-tabs", true, true, false);
  renderList(dir, "library-tabs-bar-focus", true, true, true);

  // The Reading Stats graphic in its three places, selected, and before any reading.
  render(dir, "home-a-stats", baseView(Variant::A, true, 3));
  auto aEmpty = baseView(Variant::A, false, 0);
  aEmpty.stats.hasHistory = false;
  render(dir, "home-a-stats-empty", aEmpty);
  render(dir, "home-b-stats", baseView(Variant::B, true, 3));
  auto cStats = baseView(Variant::C, true, 7);
  cStats.shelfCount = 4;
  render(dir, "home-c-stats", cStats);
  auto cEmpty = baseView(Variant::C, false, 0);
  cEmpty.shelfCount = 2;
  cEmpty.stats.hasHistory = false;
  render(dir, "home-c-stats-empty", cEmpty);

  // x4-orbit stats: the battery estimate, Reading Stats, the reader's bar.
  {
    GfxRenderer r;
    r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
    battery(r);
    StatsDraw::drawQuietText(r, 480 - 12 - 16 - 4 - r.getTextWidth(SMALL_FONT_ID, "90%") - 10, 5, "~9 days");
    const auto view = baseView(Variant::B, true, 1);
    const auto frame = OrbitHomeDraw::layout(r, view);
    OrbitHomeDraw::draw(r, view, frame);
    hints(r, "", "Open", "Left", "Right");
    writePgm(r, dir + "/home-b-battery.pgm");
  }
  renderStats(dir, "stats", 14, 41, 2 * 3600 + 10 * 60);
  renderStats(dir, "stats-few-sessions", 3, 1, 300);
  renderStats(dir, "stats-no-sessions", 0, 100, 9 * 3600);
  renderReaderBar(dir, "reader-bar", 4, false);
  renderReaderBar(dir, "reader-bar-thin-rtl", 2, true);
  return 0;
}
