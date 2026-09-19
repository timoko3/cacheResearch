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

} // namespace tests
