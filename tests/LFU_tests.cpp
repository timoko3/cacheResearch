#include "cache_tests_tools.h"

namespace tests {

TEST(CacheLFUTrace, RepeatedOne) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 1, 1, 1}, "MHHH");
}

TEST(CacheLFUTrace, AlternatingTwo) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 1, 2}, "MMHHHH");
}

TEST(CacheLFUTrace, CycleThree) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 3}, "MMMHHH");
}

TEST(CacheLFUTrace, CycleFour) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 3, 4}, "MMMMMMMM");
}

TEST(CacheLFUTrace, HotOne) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 3, 1, 2, 3}, "MMHMHHH");
}

TEST(CacheLFUTrace, MixedA) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 4, 2, 3, 4, 1}, "MMMHMMMMH");
}

TEST(CacheLFUTrace, ScanFive) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMMMMMMHHMMM");
}

TEST(CacheLFUTrace, MixedB) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 2, 4, 1, 2, 3, 4}, "MMMHHMHHMM");
}

TEST(CacheLFUTrace, HotTwo) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 2, 2, 5, 1, 2, 3, 4, 5}, "MMMMHHMMHMMM");
}

TEST(CacheLFUTrace, ReuseAfterScan) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 2}, "MMMMMMHMMMMM");
}

TEST(CacheLFUTrace, TwoHotThenScan) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "MMHHMMHHMHHMMM");
}

TEST(CacheLFUTrace, HotTwoLong) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 2, 1, 4, 2, 5, 2, 1, 3, 4, 5}, "MMMHHMHMHHMMM");
}

TEST(CacheLFUFocused, MetadataIsPreserved) {
    cache::CacheLFU<int, int> c(5, cache::L2);
    EXPECT_EQ(c.getSize(), 5u);
    EXPECT_EQ(c.getLevel(), cache::L2);
}

TEST(CacheLFUFocused, FirstMissThenHitUpdatesStats) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {8, 8}, "MH");
}

TEST(CacheLFUFocused, HotKeySurvivesEviction) {
    cache::CacheLFU<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 1, 1, 3, 1, 2}, "MMHHMHM");
}

TEST(CacheLFUFocused, LeastFrequentlyUsedPageIsEvicted) {
    cache::CacheLFU<uint32_t, int> c(3);
    lookupUpdateTest(c, {1, 2, 3, 1, 1, 2, 4, 1, 2, 3}, "MMMHHHMHHM");
}

TEST(CacheLFUFocused, EqualFrequencyUsesOldestAmongMinimumFrequency) {
    cache::CacheLFU<uint32_t, int> c(2);
    lookupUpdateTest(c, {1, 2, 3, 2, 1}, "MMMHM");
}

TEST(CacheLFUFocused, RepeatedHitsNeverCallSlowLoaderAgain) {
    cache::CacheLFU<uint32_t, int> c(2);
    lookupUpdateTest(c, {42, 42, 42, 42, 42, 42}, "MHHHHH");
}

TEST(CacheLFUFocused, CapacityOneEvictsOldKey) {
    cache::CacheLFU<uint32_t, int> c(1);
    lookupUpdateTest(c, {1, 1, 2, 2, 1}, "MHMHM");
}

TEST(CacheLFUFocused, ZeroCapacityNeverCaches) {
    cache::CacheLFU<uint32_t, int> c(0);
    lookupUpdateTest(c, {3, 3, 3}, "MMM");
}

} // namespace tests
