#include "cache/cacheREF.h"

int checkREFHeader() {
    cache::CacheREF<int> instance(2, {1});
    return instance.lookupUpdate(1, [](int key) { return key; });
}
