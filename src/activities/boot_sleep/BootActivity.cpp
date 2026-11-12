#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/OrbitArt.h"
#include "fontIds.h"

void BootActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock(*this);

  const int h = renderer.getScreenHeight();
  renderer.clearScreen();
  OrbitArt::draw(renderer);
  renderer.drawCenteredText(SMALL_FONT_ID, h - 56, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, h - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
