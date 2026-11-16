add_executable(KeyboardPressUndoTest
  KeyboardPressUndoTest.cpp
)

target_include_directories(KeyboardPressUndoTest PRIVATE
  ${REPO_ROOT}/src/activities/util
)

target_link_libraries(KeyboardPressUndoTest PRIVATE
  GTest::gtest_main
)

gtest_discover_tests(KeyboardPressUndoTest)
