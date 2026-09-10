#pragma once

#include <stddef.h>
#include <list>
#include <unordered_map>
#include <utility>

namespace cache{

enum cacheLevel
{
    L1,
    L2
};

template <typename T, typename keyT = int> 
class cache_t
{
    cacheLevel level_;
    size_t     size_;

    using Entry = std::pair<keyT, T>;
    std::list<Entry> cache_;

    using ListIt = typename std::list<Entry>::iterator;
    std::unordered_map<keyT, ListIt> hash_;
    
    struct CacheStats
    {
        size_t amountRequests = 0;
        size_t amountHits = 0;
    } stats_;

public:
    cache_t(size_t sz) : size_(sz) {}

    cacheLevel getLevel() const { return level_; }
    size_t getSize() const { return size_; }
    const std::list<Entry>& getCache() const { return cache_; }
    const std::unordered_map<keyT, ListIt>& getHash() const { return hash_; }

    const CacheStats& getStats() const { return stats_; }

    bool full() const
    {
        return cache_.size() >= size_;
    }

    template <typename F>
    bool lookupUpdate(keyT key, F slow_get_page);
};

}