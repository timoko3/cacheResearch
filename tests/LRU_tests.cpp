#include "cache_tests_tools.h"

namespace tests {

TEST(CacheLRUTrace, RepeatedOne) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(CacheLRUTrace, AlternatingTwo) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMHHHH");
}

TEST(CacheLRUTrace, CycleThree) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMHHH");
}

TEST(CacheLRUTrace, CycleFour) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMMMMM");
}

TEST(CacheLRUTrace, HotOne) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHHH");
}

TEST(CacheLRUTrace, MixedA) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMHMMMHM");
}

TEST(CacheLRUTrace, ScanFive) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMMMMHHMMM");
}

TEST(CacheLRUTrace, MixedB) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMHHMHHMM");
}

TEST(CacheLRUTrace, HotTwo) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMMHMMM");
}

TEST(CacheLRUTrace, ReuseAfterScan) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMMMHMMMMM");
}

TEST(CacheLRUTrace, TwoHotThenScan) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMHHMMMMMHHMMM");
}

TEST(CacheLRUTrace, HotTwoLong) {
    cache::CacheLRU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMHHMHMHMMMM");
}

TEST(CacheLRUFocused, MetadataAndStartsEmpty) {
    cache::CacheLRU<int, int> c(3, cache::L2);
    EXPECT_EQ(c.getSize(), 3u);
    EXPECT_EQ(c.getLevel(), cache::L2);
    EXPECT_TRUE(c.getCache().empty());
    EXPECT_TRUE(c.getHash().empty());
}

TEST(CacheLRUFocused, FirstMissInsertsAtFront) {
    cache::CacheLRU<int, int> c(3);
    int calls = 0;
    auto slow = [&](int key) { ++calls; return key * 10; };

    EXPECT_EQ(c.lookupUpdate(7, slow), 70);
    ASSERT_EQ(c.getCache().size(), 1u);
    EXPECT_EQ(c.getCache().front().first, 7);
    EXPECT_EQ(c.getCache().front().second, 70);
    EXPECT_EQ(c.getHash().size(), 1u);
    EXPECT_EQ(calls, 1);
}

TEST(CacheLRUFocused, HitMovesEntryToFront) {
    cache::CacheLRU<int, int> c(3);
    auto slow = [](int key) { return key * 10; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    c.lookupUpdate(3, slow);
    ASSERT_EQ(c.getCache().front().first, 3);

    c.lookupUpdate(1, slow);
    EXPECT_EQ(c.getCache().front().first, 1);
}

TEST(CacheLRUFocused, OverflowEvictsLeastRecentlyUsed) {
    cache::CacheLRU<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMMMM");
}

TEST(CacheLRUFocused, HitProtectsPageFromNextEviction) {
    cache::CacheLRU<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2}, "MMHMHM");
}

TEST(CacheLRUFocused, RepeatedHitsDoNotGrowCache) {
    cache::CacheLRU<int, int> c(4);
    auto slow = [](int key) { return key; };

    c.lookupUpdate(9, slow);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(c.lookupUpdate(9, slow), 9);
    }

    EXPECT_EQ(c.getCache().size(), 1u);
    EXPECT_EQ(c.getHash().size(), 1u);
}

TEST(CacheLRUFocused, CapacityOneKeepsOnlyNewestPage) {
    cache::CacheLRU<uint32_t, int> c(1);
    lookupUpdateTest(c, {1, 1, 2, 2, 1, 1}, "MHMHMH");
    ASSERT_EQ(c.getCache().size(), 1u);
    EXPECT_EQ(c.getCache().front().first, 1);
}

TEST(CacheLRUFocused, ZeroCapacityNeverCaches) {
    cache::CacheLRU<uint32_t, int> c(0);
    lookupUpdateTest(c, {5, 5, 5, 5}, "MMMM");
    EXPECT_TRUE(c.getCache().empty());
    EXPECT_TRUE(c.getHash().empty());
}

TEST(CacheLRU, ReloadedPageHasNewValue) {
    cache::CacheLRU<int, int> c(1);
    int loads = 0;
    auto slow = [&](int) { return ++loads; };

    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(loads, 3);
}

TEST(CacheLRU, RetryFailedLoad) {
    cache::CacheLRU<int, int> c(3);
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int {
        throw std::runtime_error("load failed");
    }), std::runtime_error);

    int loads = 0;
    auto slow = [&](int) { ++loads; return 420; };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(CacheLRU, VectorPage) {
    cache::CacheLRU<std::vector<int>, int> c(3);
    int loads = 0;
    auto slow = [&](int key) { ++loads; return std::vector<int>{key, key + 1}; };

    auto page = c.lookupUpdate(7, slow);
    ASSERT_EQ(page.size(), 2);
    page[0] = -1;
    page[1] = -2;
    EXPECT_EQ(page, (std::vector<int>{-1, -2}));
    EXPECT_EQ(c.lookupUpdate(7, slow), (std::vector<int>{7, 8}));
    EXPECT_EQ(loads, 1);
}

TEST(CacheLRU, StringKeys) {
    cache::CacheLRU<int, std::string> c(3);
    int loads = 0;
    auto slow = [&](const std::string& key) { ++loads; return static_cast<int>(key.size()); };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

} // namespace tests
