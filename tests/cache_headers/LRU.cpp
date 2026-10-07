#include "cache/cacheLRU.h"

int checkLRUHeader() {
    cache::CacheLRU<int> instance(2);
    return instance.lookupUpdate(1, [](int key) { return key; });
}
