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

} // namespace tests
