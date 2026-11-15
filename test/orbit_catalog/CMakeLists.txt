add_executable(OrbitCatalogTest
  OrbitCatalogTest.cpp
  ${REPO_ROOT}/src/util/OrbitCatalogFormat.cpp
  ${REPO_ROOT}/src/util/ChargeSession.cpp
  ${REPO_ROOT}/lib/Epub/Epub/BookTextSearch.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(OrbitCatalogTest PRIVATE
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/test/shims
  ${REPO_ROOT}/lib/Logging
)

target_link_libraries(OrbitCatalogTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  SaxParser
)

gtest_discover_tests(OrbitCatalogTest)
