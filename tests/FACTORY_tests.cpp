#include "cache/cacheFactory.h"
#include "cache_tests_tools.h"

namespace tests {

TEST(CacheFactory, PreservesDescriptionAndSupportsStringKeys) {
    for (auto strategy : {cache::cacheEviction_t::C_LRU,
                          cache::cacheEviction_t::C_LFU,
                          cache::cacheEviction_t::C_ARC,
                          cache::cacheEviction_t::C_2Q,
                          cache::cacheEviction_t::C_LIRS}) {
        auto c =
            cache::CacheFactory<int, std::string>::make({4, cache::cacheLevel_t::L4, strategy});
        EXPECT_EQ(c->getSize(), 4u);
        EXPECT_EQ(c->getLevel(), cache::cacheLevel_t::L4);
        int loadedPage = 70;
        auto slow = [&](const std::string&) -> int& { return loadedPage; };
        EXPECT_NE(&c->lookupUpdate("page", slow), &loadedPage);
        EXPECT_EQ(c->lookupUpdate("page", slow), 70);
        EXPECT_EQ(c->getStats().amountHits, 1u);
    }
}

TEST(CacheFactory, RejectsUnsupportedStrategies) {
    // This scoped enum has an int underlying type; deliberately test an unnamed value.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto unnamedStrategy = static_cast<cache::cacheEviction_t>(-1);
    for (auto strategy :
         {cache::cacheEviction_t::C_REF, cache::cacheEviction_t::C_UNKNOWN, unnamedStrategy}) {
        EXPECT_THROW(cache::CacheFactory<int>::make({4, cache::cacheLevel_t::L1, strategy}),
                     std::invalid_argument);
    }
}

TEST(CacheFactory, PropagatesStrategyCapacityConstraints) {
    for (auto strategy : {cache::cacheEviction_t::C_2Q, cache::cacheEviction_t::C_LIRS}) {
        EXPECT_THROW(cache::CacheFactory<int>::make({1, cache::cacheLevel_t::L1, strategy}),
                     std::invalid_argument);
    }
}

} // namespace tests
