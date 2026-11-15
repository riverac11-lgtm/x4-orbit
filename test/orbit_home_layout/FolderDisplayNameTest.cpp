#include <gtest/gtest.h>

#include "activities/home/FolderDisplayName.h"

TEST(FolderDisplayName, ShoutedNamesAreCalmedAndOthersKept) {
  EXPECT_EQ(folderDisplayName("COMPLETED"), "Completed");
  EXPECT_EQ(folderDisplayName("TO READ"), "To Read");
  EXPECT_EQ(folderDisplayName("SCI-FI_2024"), "Sci-Fi_2024");
  EXPECT_EQ(folderDisplayName("Stephen King"), "Stephen King");
  EXPECT_EQ(folderDisplayName("eBooks"), "eBooks");
  EXPECT_EQ(folderDisplayName("2024"), "2024");
  EXPECT_EQ(folderDisplayName("A"), "A");
  EXPECT_EQ(folderDisplayName("\xC3\x89TUDES"), "\xC3\x89TUDES");  // not plain ASCII: as written
}
