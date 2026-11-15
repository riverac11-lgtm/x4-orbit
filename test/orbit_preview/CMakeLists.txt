# Host preview of the Orbit themes: renders the home screens (and the Reading Stats goal and calendar, the
# reader's status bar with chapter marks) with fake data through the firmware's own
# drawing code (src/components/themes/orbit) onto the stand-in renderer here. Not a test; run it by hand:
#   cmake --build build/test --target OrbitPreview -j1 && build/test/orbit_preview/OrbitPreview <dir>
add_executable(OrbitPreview
  OrbitPreview.cpp
  ${REPO_ROOT}/src/components/themes/orbit/OrbitHomeDraw.cpp
  ${REPO_ROOT}/src/components/themes/orbit/OrbitChrome.cpp
  ${REPO_ROOT}/src/components/StatsDraw.cpp
  ${REPO_ROOT}/src/components/CardLayout.cpp
  ${REPO_ROOT}/src/util/ChapterMarks.cpp
  ${REPO_ROOT}/src/util/ReadingCalendar.cpp
  ${REPO_ROOT}/lib/GfxRenderer/TextTruncation.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${BIDI_MARK_SOURCES}
  ${REPO_ROOT}/lib/EpdFont/EpdFontFamily.cpp
  ${REPO_ROOT}/lib/EpdFont/GlyphFallback.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
  ${REPO_ROOT}/freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp
)

target_include_directories(OrbitPreview PRIVATE
  # The stand-in GfxRenderer.h must shadow lib/GfxRenderer's.
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BIDI_MARK_INCLUDES}
  ${REPO_ROOT}/test/minibidi_arabic/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/GfxRenderer
  ${REPO_ROOT}/test/shims
  ${REPO_ROOT}/freeink-sdk/libs/ui/FreeInkUI/include
)

target_link_libraries(OrbitPreview PRIVATE crosspoint_test_common)
