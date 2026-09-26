#include "cache_tests_tools.h"

namespace tests {

TEST(CacheARCTrace, RepeatedOne) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(CacheARCTrace, AlternatingTwo) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMHHHH");
}

TEST(CacheARCTrace, CycleThree) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMHHH");
}

TEST(CacheARCTrace, CycleFour) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMMMMM");
}

TEST(CacheARCTrace, HotOne) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHHH");
}

TEST(CacheARCTrace, MixedA) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMHMMMHM");
}

TEST(CacheARCTrace, ScanFive) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMMMMHHMMM");
}

TEST(CacheARCTrace, MixedB) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMHHMHHMH");
}

TEST(CacheARCTrace, HotTwo) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMMHMMM");
}

TEST(CacheARCTrace, ReuseAfterScan) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMMMHMMMMM");
}

TEST(CacheARCTrace, TwoHotThenScan) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMHHMMHHMHHMMH");
}

TEST(CacheARCTrace, HotTwoLong) {
    cache::CacheARC<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMHHMHMHHMMH");
}

TEST(CacheARCFocused, MetadataIsPreserved) {
    cache::CacheARC<int, int> c(4, cache::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::L2);
}

TEST(CacheARCFocused, ZeroCapacityIsRejected) {
    EXPECT_THROW((cache::CacheARC<int, int>(0)), std::invalid_argument);
}

TEST(CacheARCFocused, RepeatedResidentAccessesAreHits) {
    cache::CacheARC<uint32_t, int> c(2);
    lookupUpdateTest(c, {7, 7, 7, 7, 7}, "MHHHH");
}

TEST(CacheARCFocused, CapacityOneAlternatingKeysAlwaysReloadsOnChange) {
    cache::CacheARC<uint32_t, int> c(1);
    lookupUpdateTest(c, {1, 1, 2, 2, 1, 1}, "MHMHMH");
}

TEST(CacheARCFocused, HitPromotesAndProtectsPage) {
    cache::CacheARC<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 1}, "MMHMH");
}

TEST(CacheARCFocused, GhostHitReloadsThenBecomesResidentHit) {
    cache::CacheARC<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 2, 2}, "MMHMMH");
}

TEST(CacheARCFocused, MixedTraceHasExactStats) {
    cache::CacheARC<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHMH");
}

TEST(CacheARCFocused, ReloadedGhostKeepsNewVersionOnNextHit) {
    cache::CacheARC<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 2, 2, 3}, "MMHMMHH");
}

TEST(CacheARC, ReloadedPageHasNewValue) {
    cache::CacheARC<int, int> c(2);
    int loads = 0;
    auto slow = [&](int) { return ++loads; };

    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(3, slow), 3);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(loads, 4);
}

TEST(CacheARC, RetryFailedLoad) {
    cache::CacheARC<int, int> c(3);
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int {
        throw std::runtime_error("load failed");
    }), std::runtime_error);

    int loads = 0;
    auto slow = [&](int) { ++loads; return 420; };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(CacheARC, VectorPage) {
    cache::CacheARC<std::vector<int>, int> c(3);
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

TEST(CacheARC, StringKeys) {
    cache::CacheARC<int, std::string> c(3);
    int loads = 0;
    auto slow = [&](const std::string& key) { ++loads; return static_cast<int>(key.size()); };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

} // namespace tests
