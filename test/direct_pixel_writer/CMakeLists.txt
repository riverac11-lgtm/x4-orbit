add_executable(DirectPixelWriterTest DirectPixelWriterTest.cpp)
# Stubs first so the writer's <GfxRenderer.h>/<HalDisplay.h> resolve to them.
target_include_directories(DirectPixelWriterTest BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
target_include_directories(DirectPixelWriterTest PRIVATE
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/GfxRenderer)
target_link_libraries(DirectPixelWriterTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(DirectPixelWriterTest)
