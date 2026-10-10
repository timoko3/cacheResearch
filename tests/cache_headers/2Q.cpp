#include "cache/cache2Q.h"

int check2QHeader() {
    cache::Cache2Q<int> instance(2);
    int loadedPage = 0;
    return instance.lookupUpdate(1, [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    });
}
