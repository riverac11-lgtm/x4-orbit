#include "HttpDate.h"

#include <cstring>

namespace crosspoint {
namespace http_date {

namespace {

// Reads exactly `digits` decimal digits at p; false if any is missing.
bool readNumber(const char*& p, const int digits, int& out) {
  int v = 0;
  for (int i = 0; i < digits; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
    v = v * 10 + (p[i] - '0');
  }
  p += digits;
  out = v;
  return true;
}

bool expect(const char*& p, const char c) {
  if (*p != c) return false;
  ++p;
  return true;
}

bool isLeap(const int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

}  // namespace

bool parse(const char* value, time_t& out) {
  if (value == nullptr) return false;
  const char* p = value;
  while (*p == ' ') ++p;

  // "Sun, " -- the day name is checked for shape only; a wrong weekday is the server's typo, not a
  // reason to distrust the date that follows.
  static const char kDays[] = "MonTueWedThuFriSatSun";
  bool dayOk = false;
  for (int i = 0; i < 7 && !dayOk; ++i) dayOk = strncmp(p, kDays + i * 3, 3) == 0;
  if (!dayOk) return false;
  p += 3;
  if (!expect(p, ',') || !expect(p, ' ')) return false;

  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (!readNumber(p, 2, day) || !expect(p, ' ')) return false;

  static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int month = -1;
  for (int i = 0; i < 12 && month < 0; ++i) {
    if (strncmp(p, kMonths + i * 3, 3) == 0) month = i;
  }
  if (month < 0) return false;
  p += 3;

  if (!expect(p, ' ') || !readNumber(p, 4, year) || !expect(p, ' ')) return false;
  if (!readNumber(p, 2, hour) || !expect(p, ':') || !readNumber(p, 2, minute) || !expect(p, ':') ||
      !readNumber(p, 2, second)) {
    return false;
  }
  if (strncmp(p, " GMT", 4) != 0) return false;

  static const unsigned char kMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 1970 || year > 2099) return false;
  const int monthDays = kMonthDays[month] + (month == 1 && isLeap(year) ? 1 : 0);
  // 60 seconds is a leap second, which the RFC allows; it folds into the next minute.
  if (day < 1 || day > monthDays || hour > 23 || minute > 59 || second > 60) return false;

  // Days since 1970-01-01, counted directly: timegm() is not on every libc this builds against,
  // and mktime() would apply the reader's timezone.
  long days = 0;
  for (int y = 1970; y < year; ++y) days += isLeap(y) ? 366 : 365;
  for (int m = 0; m < month; ++m) days += kMonthDays[m] + (m == 1 && isLeap(year) ? 1 : 0);
  days += day - 1;
  out = static_cast<time_t>(days) * 86400 + hour * 3600 + minute * 60 + second;
  return true;
}

}  // namespace http_date
}  // namespace crosspoint
