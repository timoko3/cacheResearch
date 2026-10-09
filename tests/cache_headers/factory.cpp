#include "cache/cacheFactory.h"

int checkFactoryHeader() {
    auto instance =
        cache::CacheFactory<int>::make({2, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU});
    int loadedPage = 1;
    return instance->lookupUpdate(1, [&](int) -> int& { return loadedPage; });
}
