#include <list>
#include <stdexcept>

#include "cache.h"
#include "cacheSystem.h"

int main() {
    for (auto strategy : {cache::cacheEviction_t::C_LRU,
                          cache::cacheEviction_t::C_LFU,
                          cache::cacheEviction_t::C_ARC,
                          cache::cacheEviction_t::C_2Q,
                          cache::cacheEviction_t::C_LIRS}) {
        auto pages = cache::CacheFactory<int>::make({3, cache::cacheLevel_t::L1, strategy});
        int buffer = 0;
        auto load = [&](int key) -> int& {
            buffer = key * 10;
            return buffer;
        };
        const int& first = pages->lookupUpdate(1, load);
        pages->lookupUpdate(2, load);
        if (first != 10 || &pages->lookupUpdate(1, load) != &first) {
            throw std::runtime_error("The cache did not return its stored page");
        }
    }

    int buffer = 7;
    auto load = [&](int) -> const int& { return buffer; };
    cache::CacheREF<int> reference(2, std::list<int>{1, 1});
    const int& first = reference.lookupUpdate(1, load);
    if (&first == &buffer || &reference.lookupUpdate(1, load) != &first) {
        throw std::runtime_error("The reference cache did not store its page");
    }
    cache::CacheSystem<int> system({{{2, cache::cacheLevel_t::L1, cache::cacheEviction_t::C_LRU},
                                     {3, cache::cacheLevel_t::L2, cache::cacheEviction_t::C_ARC}}});
    const int& resident = system.lookupUpdate(1, load);
    if (&resident == &buffer || &system.lookupUpdate(1, load) != &resident) {
        throw std::runtime_error("The cache system did not return its stored page");
    }
}
