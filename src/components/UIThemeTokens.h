#pragma once

#include <FreeInkUIGfxRenderer.h>

#include "UITheme.h"
#include "components/themes/orbit/OrbitListStyle.h"

inline freeink::ui::ThemeTokens uiThemeTokens(const freeink::ui::GfxRendererTarget& target) {
  namespace fui = freeink::ui;
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();

  fui::ThemeTokens tokens = fui::themeTokensForLineHeight(target.lineHeight(fui::GfxRendererTarget::FONT_BODY));
  tokens.headerHeight = static_cast<int16_t>(metrics.headerHeight);
  tokens.footerHeight = static_cast<int16_t>(metrics.buttonHintsHeight);
  tokens.listSidePadding = static_cast<int16_t>(metrics.contentSidePadding);
  tokens.listScrollWidth = static_cast<int16_t>(metrics.scrollBarWidth);
  tokens.listScrollInset = static_cast<int16_t>(metrics.scrollBarRightOffset);
  // Orbit: see OrbitListStyle.h.
  if (GUI.refinedChrome()) OrbitListStyle::applyTokens(tokens, metrics.contentSidePadding);
  return tokens;
}