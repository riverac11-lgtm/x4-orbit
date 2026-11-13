add_executable(OpdsBrowseTest
  OpdsBrowseTest.cpp
  ${REPO_ROOT}/src/util/OpdsSavedPosition.cpp
  ${REPO_ROOT}/src/util/OpdsDownloadQueue.cpp
)

target_include_directories(OpdsBrowseTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(OpdsBrowseTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(OpdsBrowseTest)
