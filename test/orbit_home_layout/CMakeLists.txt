add_executable(OrbitHomeLayoutTest
  OrbitHomeLayoutTest.cpp
  OrbitChromeTest.cpp
  FolderDisplayNameTest.cpp
  ${REPO_ROOT}/src/components/themes/orbit/OrbitChrome.cpp
  ${REPO_ROOT}/src/components/themes/orbit/OrbitHomeDraw.cpp
  # The Reading Stats graphic on the home screens draws its ring with StatsDraw.
  ${REPO_ROOT}/src/components/StatsDraw.cpp
  ${REPO_ROOT}/src/util/ChapterMarks.cpp
  ${REPO_ROOT}/src/util/ReadingCalendar.cpp
  ${REPO_ROOT}/lib/GfxRenderer/TextTruncation.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${BIDI_MARK_SOURCES}
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/EpdFont/GlyphFallback.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(OrbitHomeLayoutTest PRIVATE
  # OrbitChrome.cpp draws through the preview's stand-in renderer; only its parsing is tested here.
  ${REPO_ROOT}/test/orbit_preview
  ${BIDI_MARK_INCLUDES}
  ${REPO_ROOT}/test/minibidi_arabic/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/GfxRenderer
  ${REPO_ROOT}/test/shims
)

target_link_libraries(OrbitHomeLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(OrbitHomeLayoutTest)
