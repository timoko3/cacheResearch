#include "cache/cache2Q.h"

int check2QHeader() {
    cache::Cache2Q<int> instance(2);
    return instance.lookupUpdate(1, [](int key) { return key; });
}
