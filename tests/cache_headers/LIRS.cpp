#include "cache/cacheLIRS.h"

int checkLIRSHeader() {
    cache::CacheLIRS<int> instance(2);
    return instance.lookupUpdate(1, [](int key) { return key; });
}
