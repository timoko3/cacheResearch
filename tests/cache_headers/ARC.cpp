#include "cache/cacheARC.h"

int checkARCHeader() {
    cache::CacheARC<int> instance(2);
    int loadedPage = 0;
    return instance.lookupUpdate(1, [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    });
}
