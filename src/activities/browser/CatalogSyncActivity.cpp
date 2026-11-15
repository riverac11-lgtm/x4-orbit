#include "CatalogSyncActivity.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "ChargeDelivery.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "SilentRestart.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// E-ink: a progress repaint costs a refresh, so at most one this often.
constexpr uint32_t kPaintEveryMs = 3000;

ChargeDelivery::Work workFor(const CatalogSyncActivity::Mode mode) {
  switch (mode) {
    case CatalogSyncActivity::Mode::Build:
      return ChargeDelivery::Work::Build;
    case CatalogSyncActivity::Mode::Update:
      return ChargeDelivery::Work::Update;
    case CatalogSyncActivity::Mode::Download:
      break;
  }
  return ChargeDelivery::Work::Download;
}

const char* problemText(const ChargeSession::Problem problem) {
  switch (problem) {
    case ChargeSession::Problem::Wifi:
      return tr(STR_WIFI_CONN_FAILED);
    case ChargeSession::Problem::Server:
      return tr(STR_DELIVERY_SERVER);
    case ChargeSession::Problem::Auth:
      return tr(STR_AUTH_FAILED);
    case ChargeSession::Problem::Card:
      return tr(STR_CATALOG_CARD);
    case ChargeSession::Problem::None:
      break;
  }
  return nullptr;
}
}  // namespace

void CatalogSyncActivity::onEnter() {
  Activity::onEnter();
  state = State::Wifi;
  requestUpdate();
  if (WiFi.status() == WL_CONNECTED) {
    state = State::Ready;
    return;
  }
  // The picker joins a saved network by itself and asks only when there is none in reach.
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             finish();
                             return;
                           }
                           state = State::Ready;
                           requestUpdate();
                         });
}

void CatalogSyncActivity::onExit() {
  Activity::onExit();
  // Wi-Fi leaves the heap fragmented: a reboot gives it back (every Wi-Fi screen does this).
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    if (backToSettings) {
      silentRestartToSettings();
    } else {
      silentRestartToOnlineLibrary();
    }
  }
}

void CatalogSyncActivity::onStatus(void* ctx, const OrbitCatalogSync::Step step, const int done, const int total,
                                   const char* title) {
  auto* self = static_cast<CatalogSyncActivity*>(ctx);
  self->step = step;
  self->done = done;
  self->total = total;
  self->bytesDone = self->bytesTotal = 0;
  if (title) {
    snprintf(self->line, sizeof(self->line), "%s", title);
  } else {
    self->line[0] = '\0';
  }
  const uint32_t now = millis();
  if (now - self->lastPaintMs >= kPaintEveryMs) {
    self->lastPaintMs = now;
    self->requestUpdate(true);
  }
}

bool CatalogSyncActivity::onKeepGoing(void* ctx) {
  auto* self = static_cast<CatalogSyncActivity*>(ctx);
  self->mappedInput.update();
  if (self->mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    self->stopRequested = true;
    self->requestUpdate(true);
  }
  return !self->stopRequested;
}

void CatalogSyncActivity::onBytes(void* ctx, const uint32_t done, const uint32_t total) {
  auto* self = static_cast<CatalogSyncActivity*>(ctx);
  self->bytesDone = done;
  self->bytesTotal = total;
  const uint32_t now = millis();
  if (now - self->lastPaintMs >= kPaintEveryMs) {
    self->lastPaintMs = now;
    self->requestUpdate(true);
  }
}

void CatalogSyncActivity::work() {
  wifiUsed = true;
  stopRequested = false;
  done = total = 0;
  bytesDone = bytesTotal = 0;
  line[0] = '\0';
  step = mode == Mode::Download ? OrbitCatalogSync::Step::Download : OrbitCatalogSync::Step::Catalog;
  {
    HalPowerManager::Lock cpu;  // TLS and parsing: full speed
    OrbitCatalogSync::Hooks hooks;
    hooks.ctx = this;
    hooks.status = &CatalogSyncActivity::onStatus;
    hooks.keepGoing = &CatalogSyncActivity::onKeepGoing;
    hooks.bytes = &CatalogSyncActivity::onBytes;
    result = ChargeDelivery::run(server, hooks, workFor(mode), onlyId);
  }
  // The Back that stopped the work is not also a Back on the result screen.
  mappedInput.update();
}

void CatalogSyncActivity::loop() {
  if (state == State::Ready) {
    {
      RenderLock lock(*this);
      state = State::Working;
    }
    requestUpdateAndWait();
    work();
    {
      RenderLock lock(*this);
      state = State::Done;
    }
    requestUpdate(true);
    return;
  }
  if (state != State::Done) return;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool retry = stopRequested || result.problem != ChargeSession::Problem::None || result.failed > 0;
  if (retry && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    // A catalog that was saved already is updated, not rebuilt, on a second try.
    if (mode == Mode::Build && result.problem == ChargeSession::Problem::None && !stopRequested) mode = Mode::Update;
    state = State::Ready;
  }
}

void CatalogSyncActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect body = UITheme::getBodyRect(renderer, true, false);
  const int midY = body.y + body.height / 2;
  const char* title = mode == Mode::Build    ? tr(STR_CATALOG_SAVE)
                      : mode == Mode::Update ? tr(STR_CATALOG_UPDATE)
                                             : tr(STR_DOWNLOADING);
  GUI.drawHeader(renderer, UITheme::getHeaderRect(renderer, true, false), title);

  const char* back = tr(STR_BACK);
  const char* confirm = "";
  char buf[96];
  switch (state) {
    case State::Wifi:
    case State::Ready:
      renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_CHECKING_WIFI));
      break;
    case State::Working: {
      if (stopRequested) {
        renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_CATALOG_STOPPED), true, EpdFontFamily::BOLD);
        back = "";
        break;
      }
      const char* what = step == OrbitCatalogSync::Step::Catalog      ? tr(STR_CATALOG_UPDATING)
                         : step == OrbitCatalogSync::Step::Collection ? tr(STR_CATALOG_SEND_CHECK)
                                                                      : tr(STR_DOWNLOADING);
      renderer.drawCenteredText(UI_10_FONT_ID, midY - 40, what, true, EpdFontFamily::BOLD);
      if (total > 0) {
        snprintf(buf, sizeof(buf), tr(STR_OPDS_QUEUE_PROGRESS), done + 1, total);
        renderer.drawCenteredText(UI_10_FONT_ID, midY - 10, buf);
      } else if (done > 0) {
        snprintf(buf, sizeof(buf), "%d", done);
        renderer.drawCenteredText(UI_10_FONT_ID, midY - 10, buf);
      }
      if (line[0]) {
        const auto text = renderer.truncatedText(UI_10_FONT_ID, line, body.width - 40);
        renderer.drawCenteredText(UI_10_FONT_ID, midY + 20, text.c_str());
      }
      if (bytesTotal > 0) {
        GUI.drawProgressBar(renderer, Rect{body.x + 50, midY + 50, body.width - 100, 20}, bytesDone, bytesTotal);
      }
      break;
    }
    case State::Done: {
      int y = midY - 30;
      if (stopRequested) {
        renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_CATALOG_STOPPED), true, EpdFontFamily::BOLD);
        y += 30;
      }
      snprintf(buf, sizeof(buf), tr(STR_CATALOG_RESULT), result.newInCatalog, result.arrived, result.failed);
      const auto summary = renderer.truncatedText(UI_10_FONT_ID, buf, body.width - 20);
      renderer.drawCenteredText(UI_10_FONT_ID, y, summary.c_str());
      if (const char* problem = problemText(result.problem)) {
        renderer.drawCenteredText(UI_10_FONT_ID, y + 30, problem, true, EpdFontFamily::BOLD);
      }
      if (stopRequested || result.problem != ChargeSession::Problem::None || result.failed > 0) {
        confirm = tr(STR_RETRY);
      }
      break;
    }
  }
  const auto labels = mappedInput.mapLabels(back, confirm, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

// ---- The delivery notice ----

void DeliveryNoticeActivity::onEnter() {
  Activity::onEnter();
  // Seen once: shown now, so not again whatever happens next.
  ChargeDelivery::markSummarySeen();
  requestUpdate();
}

void DeliveryNoticeActivity::loop() {
  // Opened in Home's place (HomeActivity), so Back goes Home rather than popping.
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    activityManager.goHome();
    return;
  }
  if (summary.waiting > 0 && mappedInput.wasPressed(MappedInputManager::Button::Confirm) && OPDS_STORE.hasServers()) {
    startActivityForResult(std::make_unique<CatalogSyncActivity>(renderer, mappedInput, OPDS_STORE.getServers().front(),
                                                                 CatalogSyncActivity::Mode::Download),
                           [](const ActivityResult&) { activityManager.goHome(); });
  }
}

void DeliveryNoticeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect body = UITheme::getBodyRect(renderer, true, false);
  GUI.drawHeader(renderer, UITheme::getHeaderRect(renderer, true, false), tr(STR_DELIVERY_TITLE));
  char text[160];
  ChargeDelivery::describe(summary, text, sizeof(text));
  int y = body.y + 40;
  // describe() gives one line per fact.
  for (char* p = text; p && *p;) {
    char* nl = strchr(p, '\n');
    if (nl) *nl = '\0';
    renderer.drawCenteredText(UI_10_FONT_ID, y, p, true, EpdFontFamily::BOLD);
    y += 32;
    p = nl ? nl + 1 : nullptr;
  }
  y += 10;
  for (uint8_t i = 0; i < summary.nameCount && i < ChargeSession::Summary::kMaxNames; ++i) {
    const auto name = renderer.truncatedText(UI_10_FONT_ID, summary.names[i], body.width - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, y, name.c_str());
    y += 28;
  }
  if (summary.arrived > summary.nameCount) renderer.drawCenteredText(UI_10_FONT_ID, y, "...");
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), summary.waiting > 0 ? tr(STR_CATALOG_GET_NOW) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
