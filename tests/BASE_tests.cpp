#include <memory>
#include <type_traits>

#include "cache.h"
#include "cache_tests_tools.h"

namespace tests {
namespace {

class CacheInterface : public testing::TestWithParam<cache::CacheEviction> {
protected:
    std::unique_ptr<cache::Cache<int>> makeCache(std::size_t capacity) {
        if (GetParam() == cache::CacheEviction::C_REF) {
            return std::make_unique<cache::CacheREF<int>>(capacity,
                                                          std::list<int>{7, 7});
        }
        return cache::CacheFactory<int>::make(
            {capacity, cache::CacheLevel::L1, GetParam()});
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
    const auto& readOnly = *c;
    static_assert(std::is_same_v<decltype(readOnly.lookup(7)),
                                 cache::Cache<int>::PageResult>);
    EXPECT_FALSE(readOnly.lookup(7));
    EXPECT_EQ(c->getStats().amountRequests, 0u);

    const int& inserted = c->lookupUpdate(7, slow);
    const auto found = readOnly.lookup(7);
    ASSERT_TRUE(found);
    EXPECT_EQ(&found->get(), &inserted);
    EXPECT_FALSE(readOnly.lookup(99));
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
    ASSERT_TRUE(readOnly.lookup(7)); // REF's request sequence is exhausted.
    EXPECT_EQ(&readOnly.lookup(7)->get(), &inserted);
}

TEST_P(CacheInterface, LookupPreservesEvictionAndGhostHistory) {
    const std::list<int> requests{1, 2, 1, 3, 4, 2, 5, 1, 3, 2, 4, 5};
    auto make = [&]() -> std::unique_ptr<cache::Cache<int>> {
        if (GetParam() == cache::CacheEviction::C_REF) {
            return std::make_unique<cache::CacheREF<int>>(3, requests);
        }
        return cache::CacheFactory<int>::make(
            {3, cache::CacheLevel::L1, GetParam()});
    };
    auto baseline = make();
    auto probed = make();
    int buffer = 0;
    auto slow = [&](int key) -> int& {
        buffer = key * 10;
        return buffer;
    };

    for (int key : requests) {
        baseline->lookupUpdate(key, slow);
        probed->lookupUpdate(key, slow);
        for (int candidate = 1; candidate <= 6; ++candidate) {
            const auto expected = baseline->lookup(candidate);
            const auto actual = probed->lookup(candidate);
            ASSERT_EQ(actual.has_value(), expected.has_value());
            if (actual) {
                EXPECT_EQ(actual->get(), expected->get());
            }
            // Repeated reads must not affect later eviction or ghost promotion.
            probed->lookup(candidate);
        }
        EXPECT_EQ(probed->getStats().amountRequests,
                  baseline->getStats().amountRequests);
        EXPECT_EQ(probed->getStats().amountHits, baseline->getStats().amountHits);
        EXPECT_EQ(probed->getStats().amountMisses,
                  baseline->getStats().amountMisses);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllStrategies, CacheInterface,
    testing::Values(cache::CacheEviction::C_LRU, cache::CacheEviction::C_LFU,
                    cache::CacheEviction::C_ARC, cache::CacheEviction::C_2Q,
                    cache::CacheEviction::C_LIRS, cache::CacheEviction::C_REF));

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
    if (GetParam() == cache::CacheEviction::C_REF) {
        c = std::make_unique<cache::CacheREF<int>>(3, std::list<int>{1, 2, 1});
    } else {
        c = cache::CacheFactory<int>::make({3, cache::CacheLevel::L1, GetParam()});
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

    auto unexpectedLoad = [](int) -> int& {
        throw std::runtime_error("Unexpected load");
    };
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(&c->lookupUpdate(1, unexpectedLoad), first);
    EXPECT_EQ(c->getStats().amountRequests, 3u);
    EXPECT_EQ(c->getStats().amountMisses, 2u);
    EXPECT_EQ(c->getStats().amountHits, 1u);
}

} // namespace
} // namespace tests
