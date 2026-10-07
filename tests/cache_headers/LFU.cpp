#include "cache/cacheLFU.h"

int checkLFUHeader() {
    cache::CacheLFU<int> instance(2);
    return instance.lookupUpdate(1, [](int key) { return key; });
}
