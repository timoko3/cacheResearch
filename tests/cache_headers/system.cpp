#include "cacheSystem.h"

int checkSystemHeader() {
    cache::CacheSystem<int> system({{{2, cache::CacheLevel::L1, cache::CacheEviction::C_LRU}}});
    int page = 1;
    return system.lookupUpdate(1, [&](int) -> int& { return page; });
}
