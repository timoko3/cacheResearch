#include "cache/cacheREF.h"

int checkREFHeader() {
    cache::CacheREF<int> instance(2, {1});
    int loadedPage = 0;
    return instance.lookupUpdate(1, [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    });
}
