#ifndef CACHE_TESTS_TOOLS_H
#define CACHE_TESTS_TOOLS_H

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "cache.h"

namespace tests {

inline uint32_t hashInt(int key) {
    uint32_t x = static_cast<uint32_t>(key);
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

template <typename CacheT>
void checkCacheStats(CacheT& cache,
                     const std::vector<int>& requests,
                     std::string_view expected) {

    const auto expectedMisses = static_cast<std::size_t>(std::count(expected.begin(),
    expected.end(), 'M'));
    const auto expectedHits   = static_cast<std::size_t>(std::count(expected.begin(),
    expected.end(), 'H'));

    const auto& stats = cache.getStats();

    EXPECT_EQ(stats.amountRequests, requests.size());
    EXPECT_EQ(stats.amountMisses, expectedMisses);
    EXPECT_EQ(stats.amountHits, expectedHits);
    EXPECT_EQ(stats.amountHits + stats.amountMisses, stats.amountRequests);
}

template <typename CacheT>
void testOneLookup(CacheT& cache,
                   std::unordered_set<int>& loadedKeys,
                   const int key,
                   const char expectedResult) {

    bool slowGetPageWasCalled = false;
    const bool wasLoadedBefore = loadedKeys.find(key) != loadedKeys.end();

    auto slowGetPage = [&](int requestedKey) {
        EXPECT_EQ(requestedKey, key);

        slowGetPageWasCalled = true;
        loadedKeys.insert(requestedKey);

        return hashInt(requestedKey);
    };

    const int actualValue = cache.lookupUpdate(key, slowGetPage);

    ASSERT_TRUE(expectedResult == 'H' || expectedResult == 'M');
    if (expectedResult == 'M') {
        EXPECT_TRUE(slowGetPageWasCalled) << ", key = " << key;
    } else {
        EXPECT_FALSE(slowGetPageWasCalled) << ", key = " << key;
        ASSERT_TRUE(wasLoadedBefore) << "expected a hit for a key that was never loaded";
    }

    EXPECT_EQ(actualValue, hashInt(key));
}

template <typename CacheT>
void lookupUpdateTest(CacheT& cache,
                      const std::vector<int>& requests,
                      std::string_view expected) {

    ASSERT_EQ(requests.size(), expected.size());

    std::unordered_set<int> loadedKeys;

    for (std::size_t i = 0; i < requests.size(); ++i) {
        testOneLookup(cache, loadedKeys, requests[i], expected[i]);
    }

    checkCacheStats(cache, requests, expected);
}

} // namespace tests

#endif /* CACHE_TESTS_TOOLS_H */
