#include <memory>
#include <type_traits>

#include "cache.h"
#include "cache_tests_tools.h"

namespace tests {
namespace {

class CacheInterface : public testing::TestWithParam<cache::cacheEviction_t> {
protected:
    std::unique_ptr<cache::Cache<int>> makeCache(std::size_t capacity) {
        if (GetParam() == cache::cacheEviction_t::C_REF) {
            return std::make_unique<cache::CacheREF<int>>(capacity, std::list<int>{7, 7});
        }
        return cache::CacheFactory<int>::make({capacity, cache::cacheLevel_t::L1, GetParam()});
    }
};

TEST_P(CacheInterface, MissAndHitReturnSameStoredCopy) {
    auto c = makeCache(3);
    int loadedPage = 70;
    int loads = 0;
    auto slow = [&](int key) -> int& {
        EXPECT_EQ(key, 7);
        ++loads;
        return loadedPage;
    };
    static_assert(std::is_same_v<decltype(c->lookupUpdate(7, slow)), const int&>);

    const int& inserted = c->lookupUpdate(7, slow);
    EXPECT_NE(&inserted, &loadedPage);
    loadedPage = 99;
    EXPECT_EQ(inserted, 70);
    const int& resident = c->lookupUpdate(7, slow);
    EXPECT_EQ(&resident, &inserted);
    EXPECT_NE(&resident, &loadedPage);
    EXPECT_EQ(resident, 70);
    EXPECT_EQ(loads, 1);
    EXPECT_EQ(c->getStats().amountRequests, 2u);
    EXPECT_EQ(c->getStats().amountMisses, 1u);
    EXPECT_EQ(c->getStats().amountHits, 1u);
}

INSTANTIATE_TEST_SUITE_P(
    AllStrategies, CacheInterface,
    testing::Values(cache::cacheEviction_t::C_LRU, cache::cacheEviction_t::C_LFU,
                    cache::cacheEviction_t::C_ARC, cache::cacheEviction_t::C_2Q,
                    cache::cacheEviction_t::C_LIRS, cache::cacheEviction_t::C_REF));

TEST_P(CacheInterface, AcceptsLoaderReturningConstReference) {
    auto c = makeCache(3);
    const int loadedPage = 70;
    auto slow = [&](int) -> const int& { return loadedPage; };
    EXPECT_NE(&c->lookupUpdate(7, slow), &loadedPage);
    EXPECT_EQ(c->lookupUpdate(7, slow), 70);
    EXPECT_EQ(c->getStats().amountHits, 1u);
}

TEST_P(CacheInterface, ZeroCapacityIsRejected) {
    EXPECT_THROW(makeCache(0), std::invalid_argument);
}

TEST_P(CacheInterface, StoredReferenceSurvivesLoaderBufferReuseAndDestruction) {
    std::unique_ptr<cache::Cache<int>> c;
    if (GetParam() == cache::cacheEviction_t::C_REF) {
        c = std::make_unique<cache::CacheREF<int>>(3, std::list<int>{1, 2, 1});
    } else {
        c = cache::CacheFactory<int>::make({3, cache::cacheLevel_t::L1, GetParam()});
    }

    const int* first = nullptr;
    {
        int buffer = 0;
        auto slow = [&](int key) -> int& {
            buffer = key * 10;
            return buffer;
        };
        first = &c->lookupUpdate(1, slow);
        EXPECT_NE(first, &buffer);
        EXPECT_EQ(c->lookupUpdate(2, slow), 20);
        EXPECT_EQ(*first, 10);
    }

    auto unexpectedLoad = [](int) -> int& { throw std::runtime_error("Unexpected load"); };
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(&c->lookupUpdate(1, unexpectedLoad), first);
    EXPECT_EQ(c->getStats().amountRequests, 3u);
    EXPECT_EQ(c->getStats().amountMisses, 2u);
    EXPECT_EQ(c->getStats().amountHits, 1u);
}

} // namespace
} // namespace tests
