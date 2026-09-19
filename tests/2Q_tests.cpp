#include "cache_tests_tools.h"

namespace tests {

TEST(Cache2QTrace, RepeatedOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(Cache2QTrace, AlternatingTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMMHHH");
}

TEST(Cache2QTrace, CycleThree) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMMMH");
}

TEST(Cache2QTrace, CycleFour) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMMMMM");
}

TEST(Cache2QTrace, HotOne) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMMMHMH");
}

TEST(Cache2QTrace, MixedA) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMMMMMHH");
}

TEST(Cache2QTrace, ScanFive) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMMMMMMMMM");
}

TEST(Cache2QTrace, MixedB) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMMMMHHMH");
}

TEST(Cache2QTrace, HotTwo) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMMHMMHMMM");
}

TEST(Cache2QTrace, ReuseAfterScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMMMMMMMMM");
}

TEST(Cache2QTrace, TwoHotThenScan) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMHMMHMMHHMMH");
}

TEST(Cache2QTrace, HotTwoLong) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMMMMHMHHMMH");
}

TEST(Cache2QFocused, MetadataAndListsStartEmpty) {
    cache::Cache2Q<int, int> c(4, cache::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::L2);
    EXPECT_TRUE(c.getA1in().empty());
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());
    EXPECT_TRUE(c.getHash().empty());
}

TEST(Cache2QFocused, CapacityBelowTwoIsRejected) {
    EXPECT_THROW((cache::Cache2Q<int, int>(0)), std::invalid_argument);
    EXPECT_THROW((cache::Cache2Q<int, int>(1)), std::invalid_argument);
}

TEST(Cache2QFocused, FirstInsertGoesToA1in) {
    cache::Cache2Q<int, int> c(4);
    auto slow = [](int key) { return key * 10; };

    EXPECT_EQ(c.lookupUpdate(5, slow), 50);
    ASSERT_EQ(c.getA1in().size(), 1u);
    EXPECT_EQ(c.getA1in().front().first, 5);
    EXPECT_TRUE(c.getA1out().empty());
    EXPECT_TRUE(c.getAm().empty());
}

TEST(Cache2QFocused, OverflowOfA1inCreatesGhostEntry) {
    cache::Cache2Q<int, int> c(4); // KIn = 1.
    auto slow = [](int key) { return key; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);

    ASSERT_EQ(c.getA1in().size(), 1u);
    ASSERT_EQ(c.getA1out().size(), 1u);
    EXPECT_EQ(c.getA1in().front().first, 2);
    EXPECT_EQ(c.getA1out().front().first, 1);
}

TEST(Cache2QFocused, GhostHitIsMissAndPromotesPageToAm) {
    cache::Cache2Q<int, int> c(4);
    int calls = 0;
    auto slow = [&](int key) { ++calls; return key * 10; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    ASSERT_EQ(calls, 2);

    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    EXPECT_EQ(calls, 3);
    ASSERT_FALSE(c.getAm().empty());
    EXPECT_EQ(c.getAm().front().first, 1);

    EXPECT_EQ(c.lookupUpdate(1, slow), 10);
    EXPECT_EQ(calls, 3);
}

TEST(Cache2QFocused, AmHitMovesEntryToFront) {
    cache::Cache2Q<int, int> c(8);
    auto slow = [](int key) { return key; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    c.lookupUpdate(3, slow);
    c.lookupUpdate(1, slow);
    c.lookupUpdate(4, slow);
    c.lookupUpdate(2, slow);

    ASSERT_GE(c.getAm().size(), 2u);
    EXPECT_EQ(c.getAm().front().first, 2);

    c.lookupUpdate(1, slow);
    EXPECT_EQ(c.getAm().front().first, 1);
}

TEST(Cache2QFocused, A1outNeverExceedsConfiguredGhostLimit) {
    cache::Cache2Q<int, int> c(4);
    auto slow = [](int key) { return key; };

    for (int key = 1; key <= 20; ++key) {
        c.lookupUpdate(key, slow);
        EXPECT_LE(c.getA1out().size(), 2u);
    }
}

TEST(Cache2QFocused, ImmediateA1inReuseIsAHit) {
    cache::Cache2Q<uint32_t, int> c(4);
    lookupUpdateTest(c, {9, 9, 9, 10, 10}, "MHHMH");
}

} // namespace tests
