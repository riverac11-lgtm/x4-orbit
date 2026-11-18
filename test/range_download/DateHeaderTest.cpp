// Host tests: a reply's Date header reaches HalClock::applyHttpDate() only while the clock is not
// NTP-fresh, with Age added for a cached reply. Over the scripted network of RangeDownloadTest.

#include <HalClock.h>
#include <SecureHttpClient.h>
#include <WiFiClient.h>
#include <gtest/gtest.h>

namespace {

struct DateHeader : ::testing::Test {
  crosspoint::SecureHttpClient http;

  void SetUp() override {
    FakeNet::reset();
    FakeClock::approximate() = true;
    FakeClock::applied() = 0;
    http.setTimeout(5000);
  }

  int get(const char* headers) {
    FakeNet::replies().push_back(
        {std::string("HTTP/1.1 200 OK\r\n") + headers + "Content-Length: 2\r\n\r\nok", FakeReply::End::KeepOpen});
    return http.get("http://books.example/opds", [](const uint8_t*, size_t) { return true; });
  }
};

TEST_F(DateHeader, HandsTheDateToAnApproximateClock) {
  EXPECT_EQ(get("Date: Wed, 18 Nov 2026 14:05:09 GMT\r\n"), 200);
  EXPECT_EQ(FakeClock::applied(), 1795010709);
}

TEST_F(DateHeader, AddsAgeForACachedReply) {
  EXPECT_EQ(get("date: Wed, 18 Nov 2026 14:05:09 GMT\r\nAge: 120\r\n"), 200);
  EXPECT_EQ(FakeClock::applied(), 1795010709 + 120);
}

TEST_F(DateHeader, LeavesAFreshClockAlone) {
  FakeClock::approximate() = false;
  EXPECT_EQ(get("Date: Wed, 18 Nov 2026 14:05:09 GMT\r\n"), 200);
  EXPECT_EQ(FakeClock::applied(), 0);
}

TEST_F(DateHeader, IgnoresAMalformedDate) {
  EXPECT_EQ(get("Date: yesterday\r\n"), 200);
  EXPECT_EQ(FakeClock::applied(), 0);
}

}  // namespace
