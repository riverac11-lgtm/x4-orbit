"""x4-orbit: compile the book-loading hot paths for speed (-O2) instead of size (-Os).

The firmware is built -Os because flash is tight. This spends some of the spare flash (about
110 KB) on the inner loops of opening a book and laying out a chapter: inflating the EPUB's
entries, the SAX tokeniser, text layout and hyphenation, glyph lookup and blitting, and image
conversion. The chapter parser and Section are left -Os: they are large, mostly branching and
card I/O, and cost ~70 KB more for little. (TJpgDec is already -O2 through its library.json.)

Only active when the env defines ORBIT_HOT_PATHS_O2; the upstream envs build exactly as before.
"""

Import("env")  # noqa: F821  # pylint: disable=undefined-variable

HOT_PATTERNS = [
    # Unzip
    "*/uzlib/src/*.c",
    "*/InflateReader/*.cpp",
    "*/ZipFile/*.cpp",
    # XML/HTML tokenising and the chapter pipeline
    "*/SaxParser/*.c",
    "*/SaxParser/*.cpp",
    "*/Epub/Epub/ParsedText.cpp",
    "*/Epub/Epub/Page.cpp",
    "*/Epub/Epub/WordShapeCache.cpp",
    "*/Epub/Epub/ShapedStore.cpp",
    "*/Epub/Epub/hyphenation/*.cpp",
    "*/Epub/Epub/BookTextSearch.cpp",
    "*/Utf8/Utf8.cpp",
    # Glyphs and pixels
    "*/GfxRenderer/GfxRenderer.cpp",
    "*/GfxRenderer/FontCacheManager.cpp",
    "*/EpdFont/EpdFont.cpp",
    "*/EpdFont/FontDecompressor.cpp",
    "*/EpdFont/SdCardFont.cpp",
    # Images other than baseline JPEG
    "*/PngDecoder/*",
    "*/Epub/Epub/converters/*.cpp",
]


def _wants_hot_paths(e):
    for flag in e.get("BUILD_FLAGS", []):
        if "ORBIT_HOT_PATHS_O2" in str(flag):
            return True
    for define in e.get("CPPDEFINES", []):
        name = define[0] if isinstance(define, (list, tuple)) else define
        if str(name) == "ORBIT_HOT_PATHS_O2":
            return True
    return False


def _speed(env, node):  # pylint: disable=redefined-outer-name
    def swap(flags):
        return [f for f in flags if str(f) != "-Os"] + ["-O2"]

    return env.Object(
        node,
        CCFLAGS=swap(env.get("CCFLAGS", [])),
        CFLAGS=swap(env.get("CFLAGS", [])),
        CXXFLAGS=swap(env.get("CXXFLAGS", [])),
    )


if _wants_hot_paths(env):  # noqa: F821
    for pattern in HOT_PATTERNS:
        env.AddBuildMiddleware(_speed, pattern)  # noqa: F821
    print("x4-orbit: -O2 for %d hot-path patterns" % len(HOT_PATTERNS))
