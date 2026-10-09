#include "cache/cacheLIRS.h"

int checkLIRSHeader() {
    cache::CacheLIRS<int> instance(2);
    int loadedPage = 0;
    return instance.lookupUpdate(1, [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    });
}
