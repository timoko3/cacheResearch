#include "cache/cacheFactory.h"

int checkFactoryHeader() {
    auto instance =
        cache::CacheFactory<int>::make({2, cache::CacheLevel::L1, cache::CacheEviction::C_LRU});
    int loadedPage = 1;
    return instance->lookupUpdate(1, [&](int) -> int& { return loadedPage; });
}
