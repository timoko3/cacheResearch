#include "cache_tests_tools.h"

namespace tests {

TEST(CacheLIRSTrace, RepeatedOne) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(CacheLIRSTrace, AlternatingTwo) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMHHHH");
}

TEST(CacheLIRSTrace, CycleThree) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMHHH");
}

TEST(CacheLIRSTrace, CycleFour) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMHHHH");
}

TEST(CacheLIRSTrace, HotOne) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHHH");
}

TEST(CacheLIRSTrace, MixedA) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMHMHHHH");
}

TEST(CacheLIRSTrace, ScanFive) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMHHHMM");
}

TEST(CacheLIRSTrace, MixedB) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMHHMHHHH");
}

TEST(CacheLIRSTrace, HotTwo) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMHHHMM");
}

TEST(CacheLIRSTrace, ReuseAfterScan) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMHMHHHMMH");
}

TEST(CacheLIRSTrace, TwoHotThenScan) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMHHMMHHMHHHMM");
}

TEST(CacheLIRSTrace, HotTwoLong) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMHHMHMHHHMM");
}

TEST(CacheLIRSFocused, MetadataIsPreserved) {
    cache::CacheLIRS<int, int> c(4, cache::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::L2);
}

TEST(CacheLIRSFocused, CapacityBelowTwoIsRejected) {
    EXPECT_THROW((cache::CacheLIRS<int, int>(0)), std::invalid_argument);
    EXPECT_THROW((cache::CacheLIRS<int, int>(1)), std::invalid_argument);
}

TEST(CacheLIRSFocused, RepeatedSinglePageBecomesAllHitsAfterFirstLoad) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {5, 5, 5, 5, 5}, "MHHHH");
}

TEST(CacheLIRSFocused, InitialResidentSetCanBeReadWithoutReload) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMHHHH");
}

TEST(CacheLIRSFocused, NewPageEvictsColdResidentHIRPage) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 5, 4}, "MMMMMM");
}

TEST(CacheLIRSFocused, ReferencedHIRCanSurviveFollowingInsertion) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 4, 5, 4, 5}, "MMMMHMHH");
}

TEST(CacheLIRSFocused, LIRPagesSurviveHIRChurn) {
    cache::CacheLIRS<uint32_t, int> c(4);
    lookupUpdateTest(c, {1, 2, 3, 4, 5, 1, 2, 3}, "MMMMMHHH");
}

TEST(CacheLIRSFocused, MinimumSupportedCapacityHandlesMixedAccesses) {
    cache::CacheLIRS<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHMM");
}

TEST(CacheLIRS, ReloadedPageHasNewValue) {
    cache::CacheLIRS<int, int> c(2);
    int loads = 0;
    auto slow = [&](int) { return ++loads; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    c.lookupUpdate(3, slow);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(loads, 4);
}

TEST(CacheLIRS, RetryFailedLoad) {
    cache::CacheLIRS<int, int> c(3);
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int {
        throw std::runtime_error("load failed");
    }), std::runtime_error);

    int loads = 0;
    auto slow = [&](int) { ++loads; return 420; };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(CacheLIRS, VectorPage) {
    cache::CacheLIRS<std::vector<int>, int> c(3);
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

TEST(CacheLIRS, StringKeys) {
    cache::CacheLIRS<int, std::string> c(3);
    int loads = 0;
    auto slow = [&](const std::string& key) { ++loads; return static_cast<int>(key.size()); };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

} // namespace tests
