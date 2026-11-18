add_executable(ChargeWarmTest
  ChargeWarmTest.cpp
  ${REPO_ROOT}/src/util/ChargeWarmState.cpp
)

target_include_directories(ChargeWarmTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(ChargeWarmTest PRIVATE
  GTest::gtest_main
)

gtest_discover_tests(ChargeWarmTest)
