#pragma once

#include <FreeInkUICore.h>
#include <components/bars/tab-bar.h>
#include <components/lists/list.h>

// The Orbit chrome of the FreeInkUI screens (Settings, the Library, every UiListActivity), as plain
// token edits so the host preview (test/orbit_preview) applies exactly what the device does.
namespace OrbitListStyle {

// The selected row is a rounded light pill inset from the edges -- the home menu's own selection --
// instead of a full-width black bar. Row heights are untouched, so each list pages and hit-tests as
// before; the text keeps its distance from the panel edge (`contentSidePadding`).
inline void applyTokens(freeink::ui::ThemeTokens& tokens, const int contentSidePadding) {
  namespace fui = freeink::ui;
  tokens.listSelectionStyle = fui::SelectionStyle::LightPill;
  tokens.listRowRadius = 8;
  tokens.listInset = 8;
  tokens.listSidePadding = static_cast<int16_t>(contentSidePadding - tokens.listInset);
}

// A row's value in the label's face, as Lyra's own lists (LyraTheme::drawList) set it: in the small
// face the "›" that marks a submenu was a speck.
inline void applyList(freeink::ui::ListProps& props, const freeink::ui::ThemeTokens& tokens) {
  props.valueText = tokens.bodyText;
}

// Tabs are words on the page, not boxes. The current one sits in a rounded pill -- black while the
// bar has the focus, light while the list has it: the two meanings the boxed bar gave -- and one
// hairline under the row closes the bar off from the list.
inline void applyTabBar(freeink::ui::TabBarProps& props, const bool barFocused) {
  namespace fui = freeink::ui;
  fui::StyleSet tabStyles;
  tabStyles.explicitlySet = true;
  tabStyles.normal.background = fui::Paint::solid(fui::Color::White);
  tabStyles.normal.foreground = fui::Paint::solid(fui::Color::Black);
  tabStyles.selected.background =
      barFocused ? fui::Paint::solid(fui::Color::Black) : fui::Paint::dither(fui::Color::LightGray);
  tabStyles.selected.foreground = fui::Paint::solid(barFocused ? fui::Color::White : fui::Color::Black);
  tabStyles.selected.radius = 10;
  tabStyles.focused = tabStyles.selected;
  tabStyles.active = tabStyles.selected;
  props.tabStyles = tabStyles;
  props.tabInset = fui::Insets{4, 4, 6, 4};
  props.divider = true;
}

}  // namespace OrbitListStyle
