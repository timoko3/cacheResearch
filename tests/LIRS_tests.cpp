#include <random>

#include "cache/cacheLIRS.h"
#include "cache_tests_tools.h"

struct LIRSHashCheckedKey {
    int value;
    inline static int failingValue = -1;
    bool operator==(const LIRSHashCheckedKey& other) const noexcept { return value == other.value; }
};

namespace std {
template <>
struct hash<LIRSHashCheckedKey> {
    size_t operator()(const LIRSHashCheckedKey& key) const {
        if (key.value == LIRSHashCheckedKey::failingValue) {
            throw runtime_error("key hashing failed");
        }
        return hash<int>{}(key.value);
    }
};
} // namespace std

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
    cache::CacheLIRS<int, int> c(4, cache::cacheLevel_t::L2);
    EXPECT_EQ(c.getSize(), 4u);
    EXPECT_EQ(c.getLevel(), cache::cacheLevel_t::L2);
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
    auto slow = [&](int) -> int& { return ++loads; };

    c.lookupUpdate(1, slow);
    c.lookupUpdate(2, slow);
    c.lookupUpdate(3, slow);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(c.lookupUpdate(2, slow), 4);
    EXPECT_EQ(loads, 4);
}

TEST(CacheLIRS, RetryFailedLoad) {
    cache::CacheLIRS<int, int> c(3);
    EXPECT_THROW(c.lookupUpdate(42, [](int) -> int& { throw std::runtime_error("load failed"); }),
                 std::runtime_error);

    int loads = 0;
    int loadedPage = 420;
    auto slow = [&](int) -> int& {
        ++loads;
        return loadedPage;
    };
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(c.lookupUpdate(42, slow), 420);
    EXPECT_EQ(loads, 1);
}

TEST(CacheLIRS, VectorPage) {
    cache::CacheLIRS<std::vector<int>, int> c(3);
    int loads = 0;
    std::vector<int> loadedPage;
    auto slow = [&](int key) -> std::vector<int>& {
        ++loads;
        loadedPage = {key, key + 1};
        return loadedPage;
    };

    auto page = c.lookupUpdate(7, slow);
    ASSERT_EQ(page.size(), 2u);
    page[0] = -1;
    page[1] = -2;
    EXPECT_EQ(page, (std::vector<int>{-1, -2}));
    EXPECT_EQ(c.lookupUpdate(7, slow), (std::vector<int>{7, 8}));
    EXPECT_EQ(loads, 1);
}

TEST(CacheLIRS, StringKeys) {
    cache::CacheLIRS<int, std::string> c(3);
    int loads = 0;
    int loadedPage = 0;
    auto slow = [&](const std::string& key) -> int& {
        ++loads;
        loadedPage = static_cast<int>(key.size());
        return loadedPage;
    };

    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("alpha", slow), 5);
    EXPECT_EQ(c.lookupUpdate("beta", slow), 4);
    EXPECT_EQ(loads, 2);
}

TEST(CacheLIRSFocused, GhostReloadPromotesAndDemotesBottomLIR) {
    cache::CacheLIRS<int> c(3);
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate(key, slow);
    EXPECT_EQ(c.getResidentCount(), 3u);
    EXPECT_EQ(c.getIndexedCount(), 4u);

    const int& restored = c.lookupUpdate(3, slow);
    EXPECT_NE(&restored, &loadedPage);
    EXPECT_EQ(&restored, &c.getResidentPages().front().page);
    loadedPage = 99;
    EXPECT_EQ(restored, 3);
    EXPECT_EQ(c.getStackS(), (std::list<int>{3, 4, 2}));
    EXPECT_EQ(c.getQueueQ(), (std::list<int>{1}));
    EXPECT_EQ(c.getLIRCount(), 2u);

    c.lookupUpdate(4, slow);
    EXPECT_EQ(c.getStackS(), (std::list<int>{4, 3}));
    EXPECT_EQ(c.getQueueQ(), (std::list<int>{2}));
    EXPECT_EQ(c.getIndexedCount(), 3u);
}

TEST(CacheLIRSFocused, ResidentHIROutsideStackNeedsTwoHitsToPromote) {
    cache::CacheLIRS<int> c(3);
    int loadedPage = 0;
    int loads = 0;
    auto slow = [&](int key) -> int& {
        ++loads;
        loadedPage = key;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4, 3})
        c.lookupUpdate(key, slow);

    c.lookupUpdate(1, slow);
    EXPECT_EQ(c.getQueueQ(), (std::list<int>{1}));
    EXPECT_EQ(c.getStackS(), (std::list<int>{1, 3, 4, 2}));
    c.lookupUpdate(1, slow);
    EXPECT_EQ(c.getQueueQ(), (std::list<int>{2}));
    EXPECT_EQ(c.getStackS(), (std::list<int>{1, 3}));
    EXPECT_EQ(c.getIndexedCount(), 3u);
    EXPECT_EQ(loads, 5);
}

TEST(CacheLIRS, BaseInterfaceReturnsSameStableResidentOnMissAndHit) {
    cache::CacheLIRS<int> c(3);
    cache::Cache<int>& base = c;
    int loadedPage = 17;
    auto slow = [&](int) -> int& { return loadedPage; };
    const int& inserted = base.lookupUpdate(1, slow);
    const int& resident = base.lookupUpdate(1, slow);
    EXPECT_EQ(&resident, &inserted);
    EXPECT_NE(&resident, &loadedPage);
    loadedPage = 99;
    for (int key : {2, 3, 4, 3})
        base.lookupUpdate(key, slow);
    EXPECT_EQ(&base.lookupUpdate(1, slow), &resident);
    EXPECT_EQ(resident, 17);
}

struct LIRSCopiedPage {
    int value;
    inline static bool failCopy = false;
    explicit LIRSCopiedPage(int pageValue) : value(pageValue) {}
    LIRSCopiedPage(const LIRSCopiedPage& other) : value(other.value) {
        if (failCopy)
            throw std::runtime_error("page copy failed");
    }
};

TEST(CacheLIRS, FailedCopyPreservesResidentsAndGhostHistory) {
    cache::CacheLIRS<LIRSCopiedPage> c(3);
    LIRSCopiedPage loadedPage(0);
    auto slow = [&](int key) -> LIRSCopiedPage& {
        loadedPage.value = key;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate(key, slow);
    const auto previousStack = c.getStackS();
    const auto previousQueue = c.getQueueQ();
    LIRSCopiedPage::failCopy = true;
    EXPECT_THROW(c.lookupUpdate(5, slow), std::runtime_error);
    EXPECT_THROW(c.lookupUpdate(3, slow), std::runtime_error);
    LIRSCopiedPage::failCopy = false;
    EXPECT_EQ(c.getStackS(), previousStack);
    EXPECT_EQ(c.getQueueQ(), previousQueue);
    EXPECT_EQ(c.getResidentCount(), 3u);
    EXPECT_EQ(c.getIndexedCount(), 4u);
    EXPECT_EQ(c.lookupUpdate(4, slow).value, 4);
    EXPECT_EQ(c.lookupUpdate(3, slow).value, 3);
}

TEST(CacheLIRSFocused, MixedRequestsPreserveResidentAndHistoryInvariants) {
    std::mt19937 random(42);
    for (std::size_t capacity : {2u, 3u, 10u, 200u}) {
        cache::CacheLIRS<int> c(capacity);
        EXPECT_EQ(c.getHIRTargetSize(), std::max<std::size_t>(1, capacity / 100));
        EXPECT_EQ(c.getLIRTargetSize() + c.getHIRTargetSize(), capacity);
        int loadedPage = 0;
        auto slow = [&](int key) -> int& {
            loadedPage = key * 10;
            return loadedPage;
        };
        for (int request = 0; request < 2000; ++request) {
            int key = static_cast<int>(random() % (capacity * 3));
            ASSERT_EQ(c.lookupUpdate(key, slow), key * 10);
            ASSERT_LE(c.getResidentCount(), capacity);
            ASSERT_LE(c.getLIRCount(), c.getLIRTargetSize());
            ASSERT_EQ(c.getResidentCount(), c.getLIRCount() + c.getQueueQ().size());
            std::unordered_set<int> indexedKeys;
            std::unordered_set<int> residentKeys;
            for (const auto& record : c.getResidentPages()) {
                ASSERT_TRUE(residentKeys.insert(record.key).second);
                ASSERT_EQ(record.page, record.key * 10);
                indexedKeys.insert(record.key);
            }
            std::unordered_set<int> stackKeys;
            for (int stackKey : c.getStackS()) {
                ASSERT_TRUE(stackKeys.insert(stackKey).second);
                indexedKeys.insert(stackKey);
            }
            std::unordered_set<int> queueKeys;
            for (int queueKey : c.getQueueQ()) {
                ASSERT_TRUE(queueKeys.insert(queueKey).second);
                ASSERT_EQ(residentKeys.count(queueKey), 1u);
            }
            ASSERT_FALSE(c.getStackS().empty());
            ASSERT_EQ(queueKeys.count(c.getStackS().back()), 0u);
            ASSERT_EQ(residentKeys.count(c.getStackS().back()), 1u);
            ASSERT_EQ(indexedKeys.size(), c.getIndexedCount());
        }
    }
}

TEST(CacheLIRS, FailedHashPreservesStateDuringEvictionAndGhostPromotion) {
    cache::CacheLIRS<int, LIRSHashCheckedKey> c(3);
    int loadedPage = 0;
    auto slow = [&](LIRSHashCheckedKey key) -> int& {
        loadedPage = key.value;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate({key}, slow);
    const auto previousStack = c.getStackS();
    const auto previousQueue = c.getQueueQ();

    auto failingEviction = [&](LIRSHashCheckedKey key) -> int& {
        LIRSHashCheckedKey::failingValue = 4;
        return slow(key);
    };
    EXPECT_THROW(c.lookupUpdate({5}, failingEviction), std::runtime_error);
    LIRSHashCheckedKey::failingValue = -1;
    EXPECT_EQ(c.getStackS(), previousStack);
    EXPECT_EQ(c.getQueueQ(), previousQueue);
    EXPECT_EQ(c.getIndexedCount(), 4u);

    auto failingPruning = [&](LIRSHashCheckedKey key) -> int& {
        LIRSHashCheckedKey::failingValue = 2;
        return slow(key);
    };
    EXPECT_THROW(c.lookupUpdate({3}, failingPruning), std::runtime_error);
    LIRSHashCheckedKey::failingValue = -1;
    EXPECT_EQ(c.getStackS(), previousStack);
    EXPECT_EQ(c.getQueueQ(), previousQueue);
    EXPECT_EQ(c.getResidentCount(), 3u);
    EXPECT_EQ(c.getIndexedCount(), 4u);
    EXPECT_EQ(c.getLIRCount(), 2u);
    EXPECT_EQ(c.lookupUpdate({3}, slow), 3);
}

TEST(CacheLIRS, FailedHashOnLIRHitKeepsStackOrder) {
    cache::CacheLIRS<int, LIRSHashCheckedKey> c(3);
    int loadedPage = 0;
    int loads = 0;
    auto slow = [&](LIRSHashCheckedKey key) -> int& {
        ++loads;
        loadedPage = key.value;
        return loadedPage;
    };
    for (int key : {1, 2, 3, 4})
        c.lookupUpdate({key}, slow);
    const auto previousStack = c.getStackS();
    const auto previousQueue = c.getQueueQ();

    // Moving bottom LIR 1 exposes LIR 2: planning pruning hashes key 2.
    LIRSHashCheckedKey::failingValue = 2;
    EXPECT_THROW(c.lookupUpdate({1}, slow), std::runtime_error);
    LIRSHashCheckedKey::failingValue = -1;
    EXPECT_EQ(c.getStackS(), previousStack);
    EXPECT_EQ(c.getQueueQ(), previousQueue);
    EXPECT_EQ(c.getResidentCount(), 3u);
    EXPECT_EQ(c.getIndexedCount(), 4u);
    EXPECT_EQ(c.getLIRCount(), 2u);
    EXPECT_EQ(loads, 4);
    EXPECT_EQ(c.lookupUpdate({1}, slow), 1);
    EXPECT_EQ(c.getStackS().front().value, 1);
    EXPECT_EQ(loads, 4);
}

} // namespace tests
