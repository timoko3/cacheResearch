#include "cache_tests_tools.h"

namespace tests {

TEST(CacheREFTrace, RepeatedOne) {
    const std::vector<int> requests{1, 1, 1, 1};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MHHH");
}

TEST(CacheREFTrace, AlternatingTwo) {
    const std::vector<int> requests{1, 2, 1, 2, 1, 2};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMHHHH");
}

TEST(CacheREFTrace, CycleThree) {
    const std::vector<int> requests{1, 2, 3, 1, 2, 3};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMHHH");
}

TEST(CacheREFTrace, CycleFour) {
    const std::vector<int> requests{1, 2, 3, 4, 1, 2, 3, 4};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMMHHMH");
}

TEST(CacheREFTrace, HotOne) {
    const std::vector<int> requests{1, 2, 1, 3, 1, 2, 3};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMHMHHH");
}

TEST(CacheREFTrace, MixedA) {
    const std::vector<int> requests{1, 2, 3, 1, 4, 2, 3, 4, 1};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMHMHHHM");
}

TEST(CacheREFTrace, ScanFive) {
    const std::vector<int> requests{1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMMHHMHHMMH");
}

TEST(CacheREFTrace, MixedB) {
    const std::vector<int> requests{1, 2, 3, 1, 2, 4, 1, 2, 3, 4};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMHHMHHMH");
}

TEST(CacheREFTrace, HotTwo) {
    const std::vector<int> requests{1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMMHHMHHMMH");
}

TEST(CacheREFTrace, ReuseAfterScan) {
    const std::vector<int> requests{1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMMHMHHMMHH");
}

TEST(CacheREFTrace, TwoHotThenScan) {
    const std::vector<int> requests{1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMHHMMHHMHHMMH");
}

TEST(CacheREFTrace, HotTwoLong) {
    const std::vector<int> requests{1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMHHMHMHHMMH");
}

TEST(CacheREFFocused, MetadataIsPreserved) {
    std::list<int> requests{1};
    cache::CacheREF<int, int> c(3, requests, cache::L2);
    EXPECT_EQ(c.getSize(), 3u);
    EXPECT_EQ(c.getLevel(), cache::L2);
}

TEST(CacheREFFocused, EmptyFutureRequestListIsRejected) {
    std::list<int> requests;
    EXPECT_THROW((cache::CacheREF<int, int>(3, requests)), std::invalid_argument);
}

TEST(CacheREFFocused, RepeatedKeyHitsAfterFirstLoad) {
    const std::vector<int> requests{4, 4, 4, 4};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(2, future);
    lookupUpdateTest(c, requests, "MHHH");
}

TEST(CacheREFFocused, EvictsPageWhoseNextUseIsFarthestAway) {
    const std::vector<int> requests{1, 2, 3, 1, 2, 3};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(2, future);
    lookupUpdateTest(c, requests, "MMMHMH");
}

TEST(CacheREFFocused, PageNeverUsedAgainIsPreferredVictim) {
    const std::vector<int> requests{1, 2, 3, 2, 3};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(2, future);
    lookupUpdateTest(c, requests, "MMMHH");
}

TEST(CacheREFFocused, CapacityOneBehavesAsSingleSlotOptimalCache) {
    const std::vector<int> requests{1, 1, 2, 2, 1};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(1, future);
    lookupUpdateTest(c, requests, "MHMHM");
}

TEST(CacheREFFocused, ZeroCapacityNeverCaches) {
    const std::vector<int> requests{1, 1, 1};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(0, future);
    lookupUpdateTest(c, requests, "MMM");
}

TEST(CacheREFFocused, MixedOptimalTraceHasExpectedStatistics) {
    const std::vector<int> requests{1, 2, 3, 1, 4, 1, 2, 3, 4};
    std::list<int> future(requests.begin(), requests.end());
    cache::CacheREF<uint32_t, int> c(3, future);
    lookupUpdateTest(c, requests, "MMMHMHHMH");
}

TEST(CacheREF, ReloadedPageHasNewValue) {
    cache::CacheREF<int, int> c(1, std::list<int>{1, 2, 1, 1});
    int loads = 0;
    auto slow = [&](int) { return ++loads; };

    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(c.lookupUpdate(1, slow), 3);
    EXPECT_EQ(loads, 3);
}

TEST(CacheREF, RetryFailedLoad) {
    cache::CacheREF<int, int> c(3, std::list<int>{42, 42, 42});
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int {
        throw std::runtime_error("load failed");
    }), std::runtime_error);

    int loads = 0;
    auto slow = [&](int) { ++loads; return 420; };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(CacheREF, VectorPage) {
    cache::CacheREF<std::vector<int>, int> c(3, std::list<int>{7, 7});
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

TEST(CacheREF, StringKeys) {
    cache::CacheREF<int, std::string> c(
        3, std::list<std::string>{"alpha", "alpha", "beta"});
    int loads = 0;
    auto slow = [&](const std::string& key) { ++loads; return static_cast<int>(key.size()); };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

TEST(CacheREF, GetOutOfRangeThrow) {
    cache::CacheREF<int, int> c(3, std::list<int>{1, 2, 3});

    auto slow = [](int key) { return key; };

    EXPECT_EQ(c.lookupUpdate(1, slow), 1);
    EXPECT_EQ(c.lookupUpdate(2, slow), 2);
    EXPECT_EQ(c.lookupUpdate(3, slow), 3);

    EXPECT_THROW(c.lookupUpdate(4, slow), std::out_of_range);
    EXPECT_THROW(c.lookupUpdate(5, slow), std::out_of_range);
}

} // namespace tests
