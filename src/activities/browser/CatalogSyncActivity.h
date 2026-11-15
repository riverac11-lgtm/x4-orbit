#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "OpdsServerStore.h"
#include "activities/Activity.h"
#include "network/OrbitCatalogSync.h"
#include "util/ChargeSession.h"

// The Online Library's network work, on screen: save the book list, update it (and pick up the
// "Send to X4" collection), or fetch the waiting list (or one book). Wi-Fi through the usual
// network picker (it joins a saved network by itself). Back stops after the book in hand. Like
// every Wi-Fi screen it reboots on its way out, which gives the heap back.
class CatalogSyncActivity final : public Activity {
 public:
  enum class Mode : uint8_t { Build, Update, Download };
  CatalogSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const OpdsServer& server, Mode mode,
                      std::string onlyId = {}, bool backToSettings = false)
      : Activity("CatalogSync", renderer, mappedInput),
        server(server),
        mode(mode),
        onlyId(std::move(onlyId)),
        backToSettings(backToSettings) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool usesWifi() const override { return true; }
  bool preventAutoSleep() override { return state != State::Done; }

 private:
  enum class State : uint8_t { Wifi, Ready, Working, Done };
  static void onStatus(void* ctx, OrbitCatalogSync::Step step, int done, int total, const char* title);
  static bool onKeepGoing(void* ctx);
  static void onBytes(void* ctx, uint32_t done, uint32_t total);
  void work();

  OpdsServer server;
  Mode mode;
  std::string onlyId;
  bool backToSettings;
  bool wifiUsed = false;
  bool stopRequested = false;
  State state = State::Wifi;
  OrbitCatalogSync::Step step = OrbitCatalogSync::Step::Catalog;
  int done = 0;
  int total = 0;
  uint32_t bytesDone = 0;
  uint32_t bytesTotal = 0;
  uint32_t lastPaintMs = 0;
  char line[96] = {};
  ChargeSession::Summary result;
  bool wifiFailed = false;
};

// The last charge-time delivery, once, in Home's place (any theme): what arrived, what failed, and
// the first titles. Confirm gets the books still waiting now (Wi-Fi); Back goes Home.
class DeliveryNoticeActivity final : public Activity {
 public:
  DeliveryNoticeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const ChargeSession::Summary& summary)
      : Activity("DeliveryNotice", renderer, mappedInput), summary(summary) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ChargeSession::Summary summary;
};
