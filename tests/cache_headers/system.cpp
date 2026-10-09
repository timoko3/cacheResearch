#include "cacheSystem.h"

int checkSystemHeader() {
    cache::CacheSystem<int> system({{{2, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU}}});
    int page = 1;
    return system.lookupUpdate(1, [&](int) -> int& { return page; });
}
