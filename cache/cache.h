#pragma once

#include <stddef.h>

#define MAX_CACHE_LEVELS 5

namespace cache {

enum cacheLevel { L1, L2, L3, L4, L5 };

enum cacheEvictionType { C_LRU, C_LFU, C_ARC, C_LIRS, C_2Q, C_REF, C_UNKNOWN };

struct cacheDescription {
    size_t size;
    cacheLevel level;
    cacheEvictionType strategy;
};

struct CacheStats {
    size_t amountRequests = 0;
    size_t amountHits = 0;
    size_t amountMisses = 0;
};

template <typename T, typename keyT = int>
class Cache {
    cacheLevel level_;
    size_t size_;

    CacheStats stats_;

public:
    explicit Cache(size_t size, cacheLevel level = L1) : level_(level), size_(size) {}

    virtual ~Cache() = default;

    cacheLevel getLevel() const { return level_; }
    size_t getSize() const { return size_; }
    const CacheStats& getStats() const { return stats_; }

    template <typename F>
    T lookupUpdate(keyT key, F slow_get_page) {
        ++stats_.amountRequests;

        if (const T* page = findAndTouch(key)) {
            ++stats_.amountHits;
            return *page;
        }

        ++stats_.amountMisses;
        T page = slow_get_page(key);

        if (size_ != 0) {
            insert(key, page);
        }

        return page;
    }

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

protected:
    // Returns a resident page, or nullptr on a miss (including ghost entries).
    virtual const T* findAndTouch(const keyT& key) = 0;

    virtual void insert(const keyT& key, T page) = 0;
};

} // namespace cache
