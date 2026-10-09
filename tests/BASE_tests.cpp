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

TEST_P(CacheInterface, MissReturnsLoaderReferenceAndHitReturnsStoredCopy) {
    auto c = makeCache(3);
    int loadedPage = 70;
    int loads = 0;
    auto slow = [&](int key) -> int& {
        EXPECT_EQ(key, 7);
        ++loads;
        return loadedPage;
    };
    static_assert(std::is_same_v<decltype(c->lookupUpdate(7, slow)), const int&>);

    EXPECT_EQ(&c->lookupUpdate(7, slow), &loadedPage);
    loadedPage = 99;
    const int& resident = c->lookupUpdate(7, slow);
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
    EXPECT_EQ(&c->lookupUpdate(7, slow), &loadedPage);
    EXPECT_EQ(c->lookupUpdate(7, slow), 70);
    EXPECT_EQ(c->getStats().amountHits, 1u);
}

} // namespace
} // namespace tests
