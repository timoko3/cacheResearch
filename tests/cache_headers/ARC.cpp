#include "cache/cacheARC.h"

int checkARCHeader() {
    cache::CacheARC<int> instance(2);
    return instance.lookupUpdate(1, [](int key) { return key; });
}
