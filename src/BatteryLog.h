#pragma once

#include <cstddef>
#include <cstdint>

// Keeps BatteryDays' log in /.crosspoint/battery-days.bin and answers "~N days of reading left".
//
// The card is written once per wake, on the way to sleep, together with the other sleep-time
// saves; the reading at wake is only held in RAM until then. Everything a screen asks for is
// answered from RAM, so the status bar and Home can ask from render().
namespace BatteryLog {

inline constexpr char kPath[] = "/.crosspoint/battery-days.bin";

// At boot, once the card is up: reads the log and remembers the battery at wake.
void begin(uint8_t wakePercent, bool cableAtWake);
// On the way to deep sleep, while the card is still up: the cycle into the log, and the log out.
void beforeSleep(uint8_t sleepPercent, bool cableNow, uint32_t awakeSeconds);
// The owner's average minutes of reading a day (ReadingCalendar::averageDailyMinutes), from the
// stats Home reads anyway; 0 leaves the last known one. Kept with the next write.
void setDailyMinutes(uint16_t minutes);

// Days left at `percent`, or 0 when there is nothing to show yet, or the cable is in.
uint8_t daysLeft(uint8_t percent, bool cableIn);
// "~9 days" into `out`; false (and an empty string) when there is nothing to show.
bool format(char* out, size_t size, uint8_t percent, bool cableIn);

}  // namespace BatteryLog
