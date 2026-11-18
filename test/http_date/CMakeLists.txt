add_executable(HttpDateTest
  HttpDateTest.cpp
  ${REPO_ROOT}/lib/SecureNet/src/HttpDate.cpp
)

target_include_directories(HttpDateTest PRIVATE
  ${REPO_ROOT}/lib/SecureNet/include
)

target_link_libraries(HttpDateTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HttpDateTest)
