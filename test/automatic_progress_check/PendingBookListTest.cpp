// Requirement: a book whose upload did not finish stays pending until its own
// upload settles, whatever happens to other books.
#include <PendingBookList.h>
#include <gtest/gtest.h>

using namespace pendingbooks;

TEST(PendingBookList, UploadOfAnotherBookKeepsTheWaitingBook) {
  std::string list = added("", "/finished.epub");
  list = added(list, "/next.epub");
  list = removed(list, "/next.epub");
  EXPECT_TRUE(contains(list, "/finished.epub"));
  EXPECT_FALSE(contains(list, "/next.epub"));
}

TEST(PendingBookList, MarkerFromTheOneBookFormatStaysValid) {
  EXPECT_TRUE(contains("/old.epub", "/old.epub"));
  EXPECT_TRUE(contains(added("/old.epub", "/new.epub"), "/old.epub"));
}

TEST(PendingBookList, PathPrefixIsNotAMatch) { EXPECT_FALSE(contains(added("", "/book.epub.bak"), "/book.epub")); }

TEST(PendingBookList, MarkingTwiceKeepsOneEntry) {
  const std::string list = added(added("", "/a.epub"), "/a.epub");
  EXPECT_EQ(parse(list).size(), 1u);
  EXPECT_TRUE(removed(list, "/a.epub").empty());
}

TEST(PendingBookList, OldestBookIsDroppedPastTheLimit) {
  std::string list;
  for (size_t i = 0; i <= MAX_BOOKS; i++) list = added(list, "/" + std::to_string(i) + ".epub");
  EXPECT_EQ(parse(list).size(), MAX_BOOKS);
  EXPECT_FALSE(contains(list, "/0.epub"));
  EXPECT_TRUE(contains(list, "/" + std::to_string(MAX_BOOKS) + ".epub"));
}

TEST(PendingBookList, RetryOfAReopenedBookCancelsItsFinishedMark) {
  const std::string list = added(added("", "/a.epub", "/body/DocFragment[9]/body"), "/a.epub");
  ASSERT_EQ(parse(list).size(), 1u);
  EXPECT_TRUE(parse(list)[0].xpath.empty());
}

TEST(PendingBookList, FinishedBookOutlivesRetriesPastTheLimit) {
  std::string list = added("", "/finished.epub", "/x");
  for (size_t i = 0; i <= MAX_BOOKS; i++) list = added(list, "/" + std::to_string(i) + ".epub");
  EXPECT_EQ(parse(list).size(), MAX_BOOKS);
  EXPECT_TRUE(contains(list, "/finished.epub"));
}

TEST(PendingBookList, RenameKeepsTheFinishedPosition) {
  const std::string list = renamed(added("", "/a.epub", "/x"), "/a.epub", "/read/a.epub");
  EXPECT_FALSE(contains(list, "/a.epub"));
  ASSERT_EQ(parse(list).size(), 1u);
  EXPECT_EQ(parse(list)[0].path, "/read/a.epub");
  EXPECT_EQ(parse(list)[0].xpath, "/x");
}

TEST(PendingBookList, OnlyAnotherFinishedBookCountsAsWaiting) {
  EXPECT_FALSE(anyFinishedExcept(added("", "/a.epub"), "/b.epub"));
  EXPECT_FALSE(anyFinishedExcept(added("", "/a.epub", "/x"), "/a.epub"));
  EXPECT_TRUE(anyFinishedExcept(added("", "/a.epub", "/x"), "/b.epub"));
}

TEST(PendingBookList, LongPathsStayWithinTheFileLimit) {
  std::string list;
  for (int i = 0; i < 8; i++) list = added(list, "/" + std::to_string(i) + std::string(1000, 'x'));
  EXPECT_LE(list.size(), MAX_BYTES);
  EXPECT_TRUE(contains(list, "/7" + std::string(1000, 'x')));
}
