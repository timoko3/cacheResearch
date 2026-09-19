#include "cache_tests_tools.h"
#include "cacheSystem.h"

namespace tests {

TEST(CacheSystemFocused, EmptyHierarchyIsRejected) {
    const cache::cacheSystemParams params;
    EXPECT_THROW((cache::CacheSystem<int, int>(params)), std::invalid_argument);
}

TEST(CacheSystemFocused, LowerLevelHitDoesNotCallSlowLoader) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_LRU;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    auto slow = [&](int key) {
        ++slowCalls;
        return key * 10;
    };

    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(system.lookupUpdate(2, slow), 20);
    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(system.lookupUpdate(1, slow), 10);
    EXPECT_EQ(slowCalls, 2);

    const auto stats = system.getStats();
    ASSERT_EQ(stats.levels.size(), 2u);

    EXPECT_EQ(stats.levels[0].level, cache::L1);
    EXPECT_EQ(stats.levels[0].stats.amountRequests, 4u);
    EXPECT_EQ(stats.levels[0].stats.amountHits, 1u);
    EXPECT_EQ(stats.levels[0].stats.amountMisses, 3u);

    EXPECT_EQ(stats.levels[1].level, cache::L2);
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
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LFU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_LRU;

    params.levels[2].size = 3;
    params.levels[2].level = cache::L3;
    params.levels[2].strategy = cache::C_ARC;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    auto slow = [&](int key) {
        ++slowCalls;
        return key + 100;
    };

    const std::vector<int> requests{1, 2, 1, 3, 2, 1};
    for (const int key : requests) {
        EXPECT_EQ(system.lookupUpdate(key, slow), key + 100);
    }

    EXPECT_EQ(slowCalls, 3);

    const auto stats = system.getStats();
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
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_LFU;

    params.levels[2].size = 2;
    params.levels[2].level = cache::L3;
    params.levels[2].strategy = cache::C_ARC;

    params.levels[3].size = 4;
    params.levels[3].level = cache::L3;
    params.levels[3].strategy = cache::C_2Q;

    params.levels[4].size = 4;
    params.levels[4].level = cache::L3;
    params.levels[4].strategy = cache::C_LIRS;

    cache::CacheSystem<int, int> system(params);
    int slowCalls = 0;
    auto slow = [&](int key) {
        ++slowCalls;
        return key;
    };

    EXPECT_EQ(system.lookupUpdate(42, slow), 42);
    EXPECT_EQ(system.lookupUpdate(42, slow), 42);
    EXPECT_EQ(slowCalls, 1);

    const auto stats = system.getStats();
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
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_LFU;

    cache::CacheSystem<std::string, std::string> system(params);
    int slowCalls = 0;
    auto slow = [&](const std::string& key) {
        ++slowCalls;
        return std::string("page:") + key;
    };

    EXPECT_EQ(system.lookupUpdate("alpha", slow), "page:alpha");
    EXPECT_EQ(system.lookupUpdate("beta", slow), "page:beta");
    EXPECT_EQ(system.lookupUpdate("alpha", slow), "page:alpha");
    EXPECT_EQ(slowCalls, 2);

    const auto stats = system.getStats();
    EXPECT_EQ(stats.total.amountRequests, 3u);
    EXPECT_EQ(stats.total.amountHits, 1u);
    EXPECT_EQ(stats.total.amountMisses, 2u);
}

TEST(CacheSystemPageTypes, VectorPages) {
    cache::cacheSystemParams params;
    params.levels.resize(2);

    params.levels[0].size = 1;
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_ARC;

    cache::CacheSystem<std::vector<int>, int> system(params);
    int slowCalls = 0;
    auto slow = [&](int key) {
        ++slowCalls;
        return std::vector<int>{key, key * key};
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
    params.levels[0].level = cache::L1;
    params.levels[0].strategy = cache::C_LRU;

    params.levels[1].size = 2;
    params.levels[1].level = cache::L2;
    params.levels[1].strategy = cache::C_LIRS;

    cache::CacheSystem<TestPage, int> system(params);
    int slowCalls = 0;
    auto slow = [&](int key) {
        ++slowCalls;
        return TestPage{key, std::string("payload-") + std::to_string(key)};
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

} // namespace tests
