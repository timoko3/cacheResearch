#include "cache.h"

cache::CacheLRU<int> checkUmbrellaHeader() {
    return cache::CacheLRU<int>(2);
}
