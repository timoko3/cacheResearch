#include "cacheSystem.h"
#include "cache_tests_tools.h"

#include <type_traits>

namespace tests {

TEST(CacheSystemFocused, EmptyHierarchyIsRejected) {
    const cache::cacheSystemParams params;
    EXPECT_THROW((cache::CacheSystem<int, int>(params)), std::invalid_argument);
}

TEST(CacheSystemFocused, LowerLevelHitDoesNotCallSlowLoader) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_LRU;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        ++slowCalls;
        loadedPage = key * 10;
        return loadedPage;
    };

    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(system.lookupUpdate(2, slow), 20);
    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(slowCalls, 2);

    const auto stats = system.getSystemStats();
    ASSERT_EQ(stats.levels.size(), 2u);

    EXPECT_EQ(stats.levels[0].level, cache::cacheLevel_t::L1);
    EXPECT_EQ(stats.levels[0].stats.amountRequests, 4u);
    EXPECT_EQ(stats.levels[0].stats.amountHits, 1u);
    EXPECT_EQ(stats.levels[0].stats.amountMisses, 3u);

    EXPECT_EQ(stats.levels[1].level, cache::cacheLevel_t::L2);
    EXPECT_EQ(stats.levels[1].stats.amountRequests, 3u);
    EXPECT_EQ(stats.levels[1].stats.amountHits, 1u);
    EXPECT_EQ(stats.levels[1].stats.amountMisses, 2u);

    EXPECT_EQ(stats.total.amountRequests, 4u);
    EXPECT_EQ(stats.total.amountHits, 2u);
    EXPECT_EQ(stats.total.amountMisses, 2u);
}

TEST(CacheSystemFocused, ThreeLevelsUseDifferentStrategies) {
    cache::cacheSystemParams params;
    params.levels.resize(3);

    params.levels[0].size = 1;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LFU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[2].size = 3;
    params.levels[2].level = cache::cacheLevel_t::L3;
    params.levels[2].strategy = cache::cacheEviction_t::C_ARC;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        ++slowCalls;
        loadedPage = key + 100;
        return loadedPage;
    };

    const std::vector<int> requests{1, 2, 1, 3, 2, 1};
    for (const int key : requests) {
        EXPECT_EQ(system.lookupUpdate(key, slow), key + 100);
    }

    EXPECT_EQ(slowCalls, 3);

    const auto stats = system.getSystemStats();
    ASSERT_EQ(stats.levels.size(), 3u);

    EXPECT_EQ(stats.levels[0].stats.amountRequests, 6u);
    EXPECT_EQ(stats.levels[0].stats.amountHits, 0u);
    EXPECT_EQ(stats.levels[0].stats.amountMisses, 6u);

    EXPECT_EQ(stats.levels[1].stats.amountRequests, 6u);
    EXPECT_EQ(stats.levels[1].stats.amountHits, 1u);
    EXPECT_EQ(stats.levels[1].stats.amountMisses, 5u);

    EXPECT_EQ(stats.levels[2].stats.amountRequests, 5u);
    EXPECT_EQ(stats.levels[2].stats.amountHits, 2u);
    EXPECT_EQ(stats.levels[2].stats.amountMisses, 3u);

    EXPECT_EQ(stats.total.amountRequests, requests.size());
    EXPECT_EQ(stats.total.amountHits, 3u);
    EXPECT_EQ(stats.total.amountMisses, 3u);
}

TEST(CacheSystemFocused, FactoryCreatesEverySupportedStrategy) {
    cache::cacheSystemParams params;
    params.levels.resize(5);

    params.levels[0].size = 2;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_LFU;

    params.levels[2].size = 2;
    params.levels[2].level = cache::cacheLevel_t::L3;
    params.levels[2].strategy = cache::cacheEviction_t::C_ARC;

    params.levels[3].size = 4;
    params.levels[3].level = cache::cacheLevel_t::L3;
    params.levels[3].strategy = cache::cacheEviction_t::C_2Q;

    params.levels[4].size = 4;
    params.levels[4].level = cache::cacheLevel_t::L3;
    params.levels[4].strategy = cache::cacheEviction_t::C_LIRS;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    int loadedPage = 0;
    auto slow = [&](int key) -> int& {
        ++slowCalls;
        loadedPage = key;
        return loadedPage;
    };

    EXPECT_EQ(system.lookupUpdate(42, slow), 42);
    EXPECT_EQ(system.lookupUpdate(42, slow), 42);
    EXPECT_EQ(slowCalls, 1);

    const auto stats = system.getSystemStats();
    ASSERT_EQ(stats.levels.size(), 5u);
    EXPECT_EQ(stats.levels.front().stats.amountRequests, 2u);
    EXPECT_EQ(stats.levels.front().stats.amountHits, 1u);

    for (std::size_t index = 1; index < stats.levels.size(); ++index) {
        EXPECT_EQ(stats.levels[index].stats.amountRequests, 1u);
        EXPECT_EQ(stats.levels[index].stats.amountHits, 0u);
        EXPECT_EQ(stats.levels[index].stats.amountMisses, 1u);
    }

    EXPECT_EQ(stats.total.amountRequests, 2u);
    EXPECT_EQ(stats.total.amountHits, 1u);
    EXPECT_EQ(stats.total.amountMisses, 1u);
}

TEST(CacheSystemPageTypes, StringPagesAndStringKeys) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_LFU;

    cache::CacheSystem<std::string, std::string> system(params);
    int slowCalls = 0;
    std::string loadedPage;
    auto slow = [&](const std::string& key) -> std::string& {
        ++slowCalls;
        loadedPage = std::string("page:") + key;
        return loadedPage;
    };

    EXPECT_EQ(system.lookupUpdate("alpha", slow), "page:alpha");
    EXPECT_EQ(system.lookupUpdate("beta", slow), "page:beta");
    EXPECT_EQ(system.lookupUpdate("alpha", slow), "page:alpha");
    EXPECT_EQ(slowCalls, 2);

    const auto stats = system.getSystemStats();
    EXPECT_EQ(stats.total.amountRequests, 3u);
    EXPECT_EQ(stats.total.amountHits, 1u);
    EXPECT_EQ(stats.total.amountMisses, 2u);
}

TEST(CacheSystemPageTypes, VectorPages) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_ARC;

    cache::CacheSystem<std::vector<int>, int> system(params);
    int slowCalls = 0;
    std::vector<int> loadedPage;
    auto slow = [&](int key) -> std::vector<int>& {
        ++slowCalls;
        loadedPage = {key, key * key};
        return loadedPage;
    };

    EXPECT_EQ(system.lookupUpdate(3, slow), (std::vector<int>{3, 9}));
    EXPECT_EQ(system.lookupUpdate(4, slow), (std::vector<int>{4, 16}));
    EXPECT_EQ(system.lookupUpdate(3, slow), (std::vector<int>{3, 9}));
    EXPECT_EQ(slowCalls, 2);
}

struct TestPage {
    int id = 0;
    std::string payload;
};

TEST(CacheSystemPageTypes, UserDefinedPageType) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::cacheLevel_t::L1;
    params.levels[0].strategy = cache::cacheEviction_t::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::cacheLevel_t::L2;
    params.levels[1].strategy = cache::cacheEviction_t::C_LIRS;

    cache::CacheSystem<TestPage, int> system(params);
    int slowCalls = 0;
    TestPage loadedPage;
    auto slow = [&](int key) -> TestPage& {
        ++slowCalls;
        loadedPage = {key, std::string("payload-") + std::to_string(key)};
        return loadedPage;
    };

    const TestPage first = system.lookupUpdate(7, slow);
    EXPECT_EQ(first.id, 7);
    EXPECT_EQ(first.payload, "payload-7");

    const TestPage second = system.lookupUpdate(8, slow);
    EXPECT_EQ(second.id, 8);
    EXPECT_EQ(second.payload, "payload-8");

    const TestPage firstAgain = system.lookupUpdate(7, slow);
    EXPECT_EQ(firstAgain.id, 7);
    EXPECT_EQ(firstAgain.payload, "payload-7");
    EXPECT_EQ(slowCalls, 2);
}

struct CopyCountedPage {
    int value;
    inline static int copies = 0;
    explicit CopyCountedPage(int pageValue) : value(pageValue) {}
    CopyCountedPage(const CopyCountedPage& other) : value(other.value) { ++copies; }
};

TEST(CacheSystemInterface, ReferencesPassThroughLevelsWithoutTemporaryCopies) {
    cache::cacheSystemParams params{{
        {1, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU},
        {2, cache::cacheLevel_t::L2, cache::cacheEviction_t::C_LRU}
    }};
    cache::CacheSystem<CopyCountedPage> system(params);
    CopyCountedPage loadedPage(0);
    int loads = 0;
    auto slow = [&](int key) -> const CopyCountedPage& {
        ++loads;
        loadedPage.value = key;
        return loadedPage;
    };
    static_assert(std::is_same_v<decltype(system.lookupUpdate(1, slow)), const CopyCountedPage&>);
    static_assert(std::is_same_v<decltype(system.getStats()), const cache::cacheStats&>);
    const auto& stats = system.getStats();

    CopyCountedPage::copies = 0;
    EXPECT_EQ(&system.lookupUpdate(1, slow), &loadedPage);
    EXPECT_EQ(CopyCountedPage::copies, 2); // One stored copy per level.
    const auto& topPage = system.lookupUpdate(1, slow);
    EXPECT_NE(&topPage, &loadedPage);
    EXPECT_EQ(CopyCountedPage::copies, 2);
    system.lookupUpdate(2, slow); // Evicts key 1 from L1, but keeps it in L2.

    const int copiesBeforePromotion = CopyCountedPage::copies;
    const auto& lowerPage = system.lookupUpdate(1, slow);
    EXPECT_NE(&lowerPage, &loadedPage);
    EXPECT_EQ(lowerPage.value, 1);
    EXPECT_EQ(CopyCountedPage::copies, copiesBeforePromotion + 1); // Only refill L1.
    EXPECT_NE(&system.lookupUpdate(1, slow), &lowerPage);
    EXPECT_EQ(CopyCountedPage::copies, copiesBeforePromotion + 1);
    EXPECT_EQ(loads, 2);
    EXPECT_EQ(stats.amountRequests, 5u);
    EXPECT_EQ(stats.amountHits, 3u);
    EXPECT_EQ(stats.amountMisses, 2u);
}

TEST(CacheSystemInterface, ZeroCapacityLevelsReturnLoaderReference) {
    cache::cacheSystemParams params{{
        {0, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU},
        {0, cache::cacheLevel_t::L2, cache::cacheEviction_t::C_LFU}
    }};
    cache::CacheSystem<int> system(params);
    int loadedPage = 7;
    auto slow = [&](int) -> int& { return loadedPage; };
    EXPECT_EQ(&system.lookupUpdate(1, slow), &loadedPage);
    EXPECT_EQ(&system.lookupUpdate(1, slow), &loadedPage);
    EXPECT_EQ(system.getStats().amountRequests, 2u);
    EXPECT_EQ(system.getStats().amountMisses, 2u);
    EXPECT_EQ(system.getStats().amountHits, 0u);
}

TEST(CacheSystemInterface, FailedLoadUpdatesStatsAndCanBeRetried) {
    cache::cacheSystemParams params{{
        {1, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU}
    }};
    cache::CacheSystem<int> system(params);
    EXPECT_THROW(system.lookupUpdate(1, [](int) -> int& {
        throw std::runtime_error("load failed");
    }), std::runtime_error);
    EXPECT_EQ(system.getStats().amountRequests, 1u);
    EXPECT_EQ(system.getStats().amountMisses, 1u);
    int loadedPage = 10;
    auto slow = [&](int) -> int& { return loadedPage; };
    EXPECT_EQ(&system.lookupUpdate(1, slow), &loadedPage);
    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(system.getStats().amountRequests, 3u);
    EXPECT_EQ(system.getStats().amountHits, 1u);
    EXPECT_EQ(system.getStats().amountMisses, 2u);
}

} // namespace tests
