#pragma once

#include <string>

#include "util/ChargeWarmState.h"

class GfxRenderer;

// Pictures prepared while charging (x4-orbit). The rules and the reasons are in
// util/ChargeWarmState.h; this is the part that touches the card and the decoders.
namespace ChargeWarm {

// Where it keeps what the reader last showed and how far the preparing has got.
constexpr char kStatePath[] = "/.crosspoint/orbit_pictures.bin";

// The reader closing a book (EpubReaderActivity::onExit, the way to sleep included): the book,
// the frame it drew in, the layout file of the chapter it was in (`sectionPath`) and the page.
// The next charge prepares from there.
void rememberReader(const std::string& bookPath, const Frame& frame, const std::string& sectionPath, int spine,
                    int page, int spineCount);

// The charging session's last step (ChargeDelivery::runHeadless), with the card up, the radio off
// and no panel: decodes the pictures of the pages remembered above, stopping when `keepGoing`
// says so, and writes down where to go on from next charge.
void prepare(GfxRenderer& renderer, bool (*keepGoing)(void*), void* ctx);

}  // namespace ChargeWarm
