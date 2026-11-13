#include "SpeedCheckActivity.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "SystemStatus.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/SpeedCheck.h"

void SpeedCheckActivity::onEnter() {
  Activity::onEnter();
  saved_ = Saved::NotYet;
  requestUpdate();
}

void SpeedCheckActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    // On the loop task, as every other card write a settings screen makes; the page is not being
    // drawn meanwhile (the render task only runs on requestUpdate()). Full clock for the card's
    // free-space walk, which is the slow part.
    HalPowerManager::Lock powerLock;
    saved_ = SpeedCheck::saveReport() ? Saved::Ok : Saved::Failed;
    requestUpdate();
  }
}

void SpeedCheckActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect contentRect = UITheme::getContentRect(renderer, /*hasBottomHints=*/true, /*hasSideHints=*/false);

  renderer.clearScreen();
  GUI.drawHeader(renderer,
                 Rect{contentRect.x, contentRect.y + metrics.topPadding, contentRect.width, metrics.headerHeight},
                 tr(STR_SPEED_CHECK), SystemStatus::firmwareVersion());

  // The Boot Diagnostics screen's grid: bold label, value column at a fixed start, both clipped.
  const int fontId = UI_10_FONT_ID;
  const int leftX = contentRect.x + metrics.verticalSpacing * 3;
  const int valueX = contentRect.x + contentRect.width * 48 / 100;
  const int valueW = contentRect.x + contentRect.width - valueX - metrics.verticalSpacing * 3;
  const int labelW = valueX - leftX - 4;
  const int wideW = contentRect.x + contentRect.width - leftX - metrics.verticalSpacing * 3;
  const int lineH = renderer.getLineHeight(fontId);
  const int rowStep = lineH + 2;
  const int subHeaderHeight = lineH + 6;
  const int hintsTop = contentRect.y + contentRect.height - metrics.buttonHintsHeight;
  int y = contentRect.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  auto room = [&](const int rows) { return y + rows * rowStep <= hintsTop; };
  auto drawSection = [&](const char* title) {
    GUI.drawSubHeader(renderer, Rect{contentRect.x, y, contentRect.width, subHeaderHeight}, title);
    y += subHeaderHeight + 2;
  };
  auto drawRow = [&](const char* label, const char* value) {
    if (!room(1)) return;
    renderer.drawText(fontId, leftX, y, renderer.truncatedText(fontId, label, labelW, EpdFontFamily::BOLD).c_str(),
                      true, EpdFontFamily::BOLD);
    renderer.drawText(fontId, valueX, y, renderer.truncatedText(fontId, value, valueW).c_str());
    y += rowStep;
  };
  // A sentence across the full width, on as many lines as it needs.
  auto drawWrapped = [&](const char* text) {
    for (const std::string& line : renderer.wrappedText(fontId, text, wideW, 3)) {
      if (!room(1)) return;
      renderer.drawText(fontId, leftX, y, line.c_str());
      y += rowStep;
    }
  };
  char buf[64];
  auto drawMs = [&](const char* label, const unsigned long ms) {
    snprintf(buf, sizeof(buf), "%lu ms", ms);
    drawRow(label, buf);
  };
  // How the chapter's pages were had, in words: this is what decides most of an open's time.
  auto drawSectionRow = [&](const char* label, const SpeedCheck::SectionTiming& s, const unsigned long ms) {
    const char* how = s.source == SpeedCheck::Source::Cached      ? tr(STR_SPEED_READY)
                      : s.source == SpeedCheck::Source::BuiltLive ? tr(STR_SPEED_BUILDING)
                                                                  : tr(STR_SPEED_BUILT);
    if (s.source == SpeedCheck::Source::Cached || !s.buildDone) {
      snprintf(buf, sizeof(buf), "%lu ms, %s", ms, how);
    } else {
      snprintf(buf, sizeof(buf), "%lu ms, %s (%u p.)", ms, how, s.pages);
    }
    drawRow(label, buf);
  };

  const SpeedCheck::Log& log = SpeedCheck::log();
  const SpeedCheck::OpenRecord& open = log.lastOpen();
  const SpeedCheck::ChapterRecord& chapter = log.lastChapter();

  if (!open.valid && !chapter.valid && log.turnCount() == 0) {
    drawWrapped(tr(STR_SPEED_NONE));
    y += rowStep;
  }

  if (open.valid) {
    drawSection(tr(STR_SPEED_OPEN));
    drawMs(tr(STR_SPEED_OPEN_TOTAL), open.totalMs);
    drawMs(tr(STR_SPEED_OPEN_LOAD), open.loadMs);
    drawMs(tr(STR_SPEED_OPEN_SETUP), open.setupMs);
    drawSectionRow(tr(STR_SPEED_OPEN_SECTION), open.section, open.sectionMs);
    // A first page drawn out of a build still running has its drawing inside the row above.
    if (open.section.source != SpeedCheck::Source::BuiltLive) drawMs(tr(STR_SPEED_OPEN_DRAW), open.drawMs);
    if (open.fromWake) drawMs(tr(STR_SPEED_WAKE), open.sinceBootMs);
  }

  if (chapter.valid) {
    drawSection(tr(STR_SPEED_CHAPTER));
    drawSectionRow(tr(STR_SPEED_CHAPTER_TOTAL), chapter.section, chapter.totalMs);
  }

  if (log.turnCount() > 0) {
    drawSection(tr(STR_SPEED_TURNS));
    const SpeedCheck::Stats total = log.turnStats(SpeedCheck::TurnField::Total);
    snprintf(buf, sizeof(buf), "%u ms (%u)", total.median, total.count);
    drawRow(tr(STR_SPEED_TURN_TYPICAL), buf);
    snprintf(buf, sizeof(buf), "%u / %u ms", total.min, total.max);
    drawRow(tr(STR_SPEED_TURN_RANGE), buf);
    drawMs(tr(STR_SPEED_TURN_WAIT), log.turnStats(SpeedCheck::TurnField::Wait).median);
    drawMs(tr(STR_SPEED_TURN_RENDER), log.turnStats(SpeedCheck::TurnField::Render).median);
    drawMs(tr(STR_SPEED_TURN_REFRESH), log.turnStats(SpeedCheck::TurnField::Refresh).median);
    snprintf(buf, sizeof(buf), "%u / %u", log.preDrawnTurns(), log.turnCount());
    drawRow(tr(STR_SPEED_TURN_AHEAD), buf);
  }

  y += rowStep / 2;
  drawWrapped(saved_ == Saved::Ok       ? tr(STR_SPEED_SAVED)
              : saved_ == Saved::Failed ? tr(STR_SPEED_SAVE_FAILED)
                                        : tr(STR_SPEED_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SPEED_SAVE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
