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
//
// The same screen tells the last charge-time delivery, once, in Home's place (any theme): what
// arrived, what failed, the first titles. There Confirm gets the books still waiting now (Wi-Fi)
// and Back goes Home. One class for both: their result screens are the same, and flash is short.
//
// `server` is referenced, not copied: it is an OPDS_STORE entry (or the Online Library's), which
// outlives this screen.
class CatalogSyncActivity final : public Activity {
 public:
  enum class Mode : uint8_t { Build, Update, Download, Notice };
  CatalogSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const OpdsServer& server, Mode mode,
                      std::string onlyId = {}, bool backToSettings = false)
      : Activity("CatalogSync", renderer, mappedInput),
        server(server),
        mode(mode),
        onlyId(std::move(onlyId)),
        backToSettings(backToSettings) {}
  // The delivery notice: `summary` as the result, nothing run.
  CatalogSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const OpdsServer& server,
                      const ChargeSession::Summary& summary)
      : Activity("CatalogSync", renderer, mappedInput),
        server(server),
        mode(Mode::Notice),
        fromNotice(true),
        state(State::Done),
        result(summary) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool usesWifi() const override { return mode != Mode::Notice; }
  bool preventAutoSleep() override { return state != State::Done; }

 private:
  enum class State : uint8_t { Wifi, Ready, Working, Done };
  static void onStatus(void* ctx, OrbitCatalogSync::Step step, int done, int total, const char* title);
  static bool onKeepGoing(void* ctx);
  static void onBytes(void* ctx, uint32_t done, uint32_t total);
  void startWifi();
  void work();
  void leave();

  const OpdsServer& server;
  Mode mode;
  std::string onlyId;
  bool backToSettings = false;
  bool fromNotice = false;
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
};
