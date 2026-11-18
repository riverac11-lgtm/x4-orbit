// BookCache::clear(): what a clear of a book's cache folder removes and what it leaves. Runs over a
// real folder through the stdio storage shim.
#include <BookCache.h>
#include <CompletedLedger.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

class BookCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = (fs::path(BOOK_CACHE_ROOT) / info->name() / "epub_12345").generic_string();
    fs::remove_all(fs::path(dir_).parent_path());
    fs::create_directories(dir_);
  }
  void TearDown() override { fs::remove_all(fs::path(dir_).parent_path()); }

  void file(const std::string& rel, const std::string& content = "x") const {
    const fs::path path = fs::path(dir_) / rel;
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << content;
  }
  bool has(const std::string& rel) const { return fs::exists(fs::path(dir_) / rel); }
  std::string read(const std::string& rel) const {
    std::ifstream in(fs::path(dir_) / rel, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  }

  // What a book's cache folder holds after it was read for a while.
  void fillLikeAReadBook() const {
    file("book.bin");
    file("css_rules.cache");
    file("fingerprint.bin");
    file("pages.bin");
    file("spines/0/3_1a2b3c4d.bin");
    file("spines/0/html_3.bin");
    file("img/0.1bit.pxc");
    file("thumb_400.bmp");
    file("cover.img");
    file("kosync_push.bin");
    file("koreader_docid.txt");
    file("progress.bin", "position");
    file("linkstack.bin", "back stack");
    file("bookmarks.bin", "marks");
    file("koreader_syncmethod.txt", "1");
  }

  std::string dir_;
};

TEST_F(BookCacheTest, KeepNothingRemovesTheFolder) {
  fillLikeAReadBook();
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::Nothing));
  EXPECT_FALSE(fs::exists(dir_));
}

TEST_F(BookCacheTest, KeepReadingStateLeavesOnlyTheReadersFiles) {
  fillLikeAReadBook();
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::ReadingState));

  EXPECT_EQ("position", read("progress.bin"));
  EXPECT_EQ("back stack", read("linkstack.bin"));
  EXPECT_EQ("marks", read("bookmarks.bin"));
  EXPECT_EQ("1", read("koreader_syncmethod.txt"));
  for (const char* derived : {"book.bin", "css_rules.cache", "fingerprint.bin", "pages.bin", "spines", "img",
                              "thumb_400.bmp", "cover.img", "kosync_push.bin", "koreader_docid.txt"}) {
    EXPECT_FALSE(has(derived)) << derived << " is derived from the book and must go";
  }
}

TEST_F(BookCacheTest, KeepCoversLeavesTheImagesAndNothingElse) {
  fillLikeAReadBook();
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::Nothing, /*keepCovers=*/true));

  EXPECT_TRUE(has("thumb_400.bmp"));
  EXPECT_TRUE(has("cover.img"));
  EXPECT_FALSE(has("progress.bin"));
  EXPECT_FALSE(has("linkstack.bin"));
  EXPECT_FALSE(has("bookmarks.bin"));
  EXPECT_FALSE(has("koreader_syncmethod.txt"));
  EXPECT_FALSE(has("spines"));
}

// A book never opened, or opened on a firmware that kept no reading state, leaves no empty folder.
TEST_F(BookCacheTest, AFolderWithNothingToKeepGoes) {
  file("book.bin");
  file("spines/0/0_00000000.bin");
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::ReadingState));
  EXPECT_FALSE(fs::exists(dir_));
}

TEST_F(BookCacheTest, AMissingFolderIsClearAlready) {
  fs::remove_all(dir_);
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::ReadingState));
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::Nothing));
}

// A finished book in /COMPLETED: the heavy layout and images go, and everything the reader, Home,
// the Library, the sleep screen and KOReader sync read without reopening the book stays.
TEST_F(BookCacheTest, KeepFinishedLeavesStateMetadataAndThumbs) {
  fillLikeAReadBook();
  file("details.bin");
  file("pagelist.bin");
  file("toc.retry");
  file("cover.bmp");
  file("cover_crop.bmp");
  file("thumb_120x180.bmp", "");  // a "no cover" sentinel
  file("img_3_1a2b.jpg");
  file("img_3_1a2b.jpg.1bit.pxc");
  file("img_9_77.bmp");  // a BMP image of the book itself, not a cover
  file("search.bin");
  file("fonts/f0.bin");
  file("fonts.bin");
  EXPECT_TRUE(BookCache::clear(dir_, BookCache::Keep::Finished));

  EXPECT_EQ("position", read("progress.bin"));
  EXPECT_EQ("back stack", read("linkstack.bin"));
  EXPECT_EQ("marks", read("bookmarks.bin"));
  EXPECT_EQ("1", read("koreader_syncmethod.txt"));
  for (const char* kept : {"book.bin", "fingerprint.bin", "pagelist.bin", "toc.retry", "details.bin",
                           "koreader_docid.txt", "thumb_400.bmp", "thumb_120x180.bmp", "cover.bmp", "cover_crop.bmp"}) {
    EXPECT_TRUE(has(kept)) << kept << " is what shows the finished book and must stay";
  }
  for (const char* heavy : {"spines", "img", "img_3_1a2b.jpg", "img_3_1a2b.jpg.1bit.pxc", "img_9_77.bmp", "cover.img",
                            "css_rules.cache", "pages.bin", "search.bin", "fonts", "fonts.bin", "kosync_push.bin"}) {
    EXPECT_FALSE(has(heavy)) << heavy << " is made again on the next open and must go";
  }
}

TEST(BookCacheNames, KeptWhenFinished) {
  EXPECT_TRUE(BookCache::isKeptWhenFinished("progress.bin"));
  EXPECT_TRUE(BookCache::isKeptWhenFinished("index.bin"));
  EXPECT_TRUE(BookCache::isKeptWhenFinished("thumb_225.bmp"));
  EXPECT_FALSE(BookCache::isKeptWhenFinished("thumb_225.bmp.part"));
  EXPECT_FALSE(BookCache::isKeptWhenFinished("cover.img"));
  EXPECT_FALSE(BookCache::isKeptWhenFinished("cover.t2bit"));
  EXPECT_FALSE(BookCache::isKeptWhenFinished("page_3.bw1"));
  EXPECT_FALSE(BookCache::isKeptWhenFinished("spines"));
}

TEST(CompletedLedger, InCompleted) {
  EXPECT_TRUE(CompletedLedger::isInCompleted("/COMPLETED/Book.epub"));
  EXPECT_TRUE(CompletedLedger::isInCompleted("/COMPLETED/Series/Book.epub"));
  EXPECT_FALSE(CompletedLedger::isInCompleted("/COMPLETED"));
  EXPECT_FALSE(CompletedLedger::isInCompleted("/Books/COMPLETED/Book.epub"));
  EXPECT_FALSE(CompletedLedger::isInCompleted("/COMPLETEDX/Book.epub"));
}

TEST(CompletedLedger, KeyIsStableFnv1a) {
  EXPECT_EQ(2166136261u, CompletedLedger::pathKey(""));
  EXPECT_EQ(0xe40c292cu, CompletedLedger::pathKey("a"));
  EXPECT_NE(CompletedLedger::pathKey("/COMPLETED/a.epub"), CompletedLedger::pathKey("/COMPLETED/b.epub"));
}

TEST(CompletedLedger, SortedBoundedSet) {
  uint32_t keys[3];
  size_t count = 0;
  EXPECT_TRUE(CompletedLedger::insert(keys, count, 3, 50));
  EXPECT_TRUE(CompletedLedger::insert(keys, count, 3, 10));
  EXPECT_TRUE(CompletedLedger::insert(keys, count, 3, 50));  // already there
  EXPECT_TRUE(CompletedLedger::insert(keys, count, 3, 30));
  ASSERT_EQ(3u, count);
  EXPECT_EQ(10u, keys[0]);
  EXPECT_EQ(30u, keys[1]);
  EXPECT_EQ(50u, keys[2]);
  EXPECT_FALSE(CompletedLedger::insert(keys, count, 3, 20));  // full
  EXPECT_TRUE(CompletedLedger::insert(keys, count, 3, 30));   // full, but known
  EXPECT_EQ(3u, count);
  EXPECT_TRUE(CompletedLedger::contains(keys, count, 30));
  EXPECT_FALSE(CompletedLedger::contains(keys, count, 20));
}

}  // namespace
