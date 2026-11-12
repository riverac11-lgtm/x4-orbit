add_executable(SpeedCheckTest
  SpeedCheckTest.cpp
)

# Pure C++: SpeedCheckLog.h depends on nothing but <cstdint>/<cstdio>, and LookaheadBudget.h on
# nothing at all, so no Arduino shim, no HAL, no SD card.
target_include_directories(SpeedCheckTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_link_libraries(SpeedCheckTest PRIVATE
  GTest::gtest_main
)

gtest_discover_tests(SpeedCheckTest)
