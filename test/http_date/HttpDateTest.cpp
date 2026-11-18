// Host tests for the HTTP "Date:" header parser behind the clock fallback (lib/SecureNet HttpDate).

#include <HttpDate.h>
#include <gtest/gtest.h>

namespace hd = crosspoint::http_date;

namespace {
time_t parsed(const char* s) {
  time_t t = -1;
  EXPECT_TRUE(hd::parse(s, t)) << s;
  return t;
}
bool refused(const char* s) {
  time_t t = 12345;
  const bool ok = hd::parse(s, t);
  EXPECT_EQ(t, 12345) << "out must stay untouched on refusal: " << s;
  return !ok;
}
}  // namespace

TEST(HttpDate, ParsesTheRfcExample) { EXPECT_EQ(parsed("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777); }

TEST(HttpDate, ParsesTheEpochAndRecentDates) {
  EXPECT_EQ(parsed("Thu, 01 Jan 1970 00:00:00 GMT"), 0);
  EXPECT_EQ(parsed("Wed, 18 Nov 2026 14:05:09 GMT"), 1795010709);
  EXPECT_EQ(parsed("Tue, 31 Dec 2024 23:59:59 GMT"), 1735689599);
  EXPECT_EQ(parsed("Wed, 01 Jan 2025 00:00:00 GMT"), 1735689600);
}

TEST(HttpDate, HandlesLeapDays) {
  EXPECT_EQ(parsed("Thu, 29 Feb 2024 12:00:00 GMT"), 1709208000);
  EXPECT_EQ(parsed("Tue, 29 Feb 2000 00:00:00 GMT"), 951782400);
  EXPECT_TRUE(refused("Sun, 29 Feb 2026 00:00:00 GMT"));
  EXPECT_TRUE(refused("Mon, 29 Feb 2100 00:00:00 GMT"));
}

TEST(HttpDate, SkipsLeadingSpacesAndIgnoresTrailingText) {
  EXPECT_EQ(parsed("  Sun, 06 Nov 1994 08:49:37 GMT"), 784111777);
  EXPECT_EQ(parsed("Sun, 06 Nov 1994 08:49:37 GMT\r"), 784111777);
}

TEST(HttpDate, AcceptsALeapSecond) { EXPECT_EQ(parsed("Wed, 31 Dec 2016 23:59:60 GMT"), 1483228800); }

TEST(HttpDate, RefusesObsoleteForms) {
  EXPECT_TRUE(refused("Sunday, 06-Nov-94 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun Nov  6 08:49:37 1994"));
}

TEST(HttpDate, RefusesMalformedValues) {
  EXPECT_TRUE(refused(""));
  EXPECT_TRUE(refused("garbage"));
  EXPECT_TRUE(refused("Sun, 6 Nov 1994 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 06 nov 1994 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 06 Nov 1994 08:49:37 UTC"));
  EXPECT_TRUE(refused("Sun, 06 Nov 1994 08:49:37"));
  EXPECT_TRUE(refused("Sun, 06 Nov 1994 24:00:00 GMT"));
  EXPECT_TRUE(refused("Sun, 06 Nov 1994 08:60:00 GMT"));
  EXPECT_TRUE(refused("Sun, 31 Nov 1994 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 00 Nov 1994 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 06 Nov 1969 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 06 Nov 2100 08:49:37 GMT"));
  EXPECT_TRUE(refused("Sun, 06 Nov 19x4 08:49:37 GMT"));
  time_t t = 7;
  EXPECT_FALSE(hd::parse(nullptr, t));
  EXPECT_EQ(t, 7);
}
