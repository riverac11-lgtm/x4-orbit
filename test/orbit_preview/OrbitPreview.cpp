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

#include "components/icons/stats.h"
#include "components/themes/orbit/OrbitChrome.h"
#include "components/themes/orbit/OrbitHomeDraw.h"
#include "components/themes/orbit/OrbitListStyle.h"
#include "fontIds.h"

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
  static const char* labels[] = {"Library", "Online Library", "More", "Settings", "Reading Stats", "Bookmarks"};
  static const UIIcon icons[] = {UIIcon::Folder,   UIIcon::Library, UIIcon::Ellipsis,
                                 UIIcon::Settings, UIIcon::Stats,   UIIcon::Book};
  view.menuCount = 4;
  for (int i = 0; i < 6; i++) {
    view.menuLabel[i] = labels[i];
    view.menuIcon[i] = icons[i];
    view.menuBitmap[i] = StatsIcon;
  }
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
void renderList(const std::string& dir, const std::string& name, const bool orbit) {
  namespace fui = freeink::ui;
  GfxRenderer r;
  r.fonts = {{SMALL_FONT_ID, smallFamily}, {UI_10_FONT_ID, ui10Family}, {UI_12_FONT_ID, ui12Family}};
  battery(r);
  r.drawText(UI_12_FONT_ID, 20, 5 + 40 + 3, "Settings", true, EpdFontFamily::BOLD);
  const char* version = "2.41-orbit.3";
  r.drawText(SMALL_FONT_ID, 460 - r.getTextWidth(SMALL_FONT_ID, version), 5 + 40 + 10, version);
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
  fui::TabBarProps tabProps;
  tabProps.tabs = tabs;
  tabProps.count = 4;
  tabProps.action = 1;
  tabProps.text = tokens.bodyText;
  tabProps.tabInset = fui::Insets{};
  tabProps.contentInset = fui::Insets{};
  if (orbit) {
    OrbitListStyle::applyTabBar(tabProps, false);
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
  list.selectedIndex = 3;
  list.action = 2;
  list.labelText = tokens.bodyText;
  list.labelText.maxLines = 2;
  if (orbit) OrbitListStyle::applyList(list, tokens);
  const fui::ListProps resolved = screen.resolveListProps(list);
  fui::list(frame, fui::Rect{0, 5 + 84 + 54 + 16, 480 - 40, 800 - 40 - 8 - (5 + 84 + 54 + 16)}, resolved);

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
  return 0;
}
