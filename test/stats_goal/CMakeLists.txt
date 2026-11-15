# The pure parts of x4-orbit's reading goal, battery days and chapter marks.
add_executable(StatsGoalTest
  ReadingCalendarTest.cpp
  BatteryDaysTest.cpp
  ChapterMarksTest.cpp
  ${REPO_ROOT}/src/util/ReadingCalendar.cpp
  ${REPO_ROOT}/src/util/BatteryDays.cpp
  ${REPO_ROOT}/src/util/ChapterMarks.cpp
)

target_include_directories(StatsGoalTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(StatsGoalTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(StatsGoalTest)
