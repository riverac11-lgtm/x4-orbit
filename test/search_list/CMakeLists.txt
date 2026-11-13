add_executable(SearchListTest
  SearchListTest.cpp
)

target_include_directories(SearchListTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(SearchListTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SearchListTest)
