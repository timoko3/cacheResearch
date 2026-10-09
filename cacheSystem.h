#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "cache/cache.h"
#include "cache/cache2Q.h"
#include "cache/cacheARC.h"
#include "cache/cacheLFU.h"
#include "cache/cacheLIRS.h"
#include "cache/cacheLRU.h"

namespace cache {

struct cacheSystemParams {
    // Descriptions are ordered from the first cache level to the last.
    std::vector<cacheDescription> levels;
};

struct CacheLevelStats {
    cacheLevel_t level;
    cacheStats stats;
};

struct CacheSystemStats {
    cacheStats total;
    std::vector<CacheLevelStats> levels;
};

template <typename T, typename keyT = int>
class CacheSystem {
    std::vector<std::unique_ptr<Cache<T, keyT>>> cacheSys_;
    cacheStats stats_;

    template <typename F>
    const T& lookupUpdate(std::size_t index, keyT key, F& slow_get_page) {
        if (index == cacheSys_.size()) {
            return slow_get_page(key);
        }

        return cacheSys_[index]->lookupUpdate(
            key, [this, index, &slow_get_page](keyT requestedKey) -> const T& {
                return lookupUpdate(index + 1, requestedKey, slow_get_page);
            });
    }

    void updateStats() noexcept {
        stats_ = {};
        for (const auto& level : cacheSys_) {
            stats_.amountHits += level->getStats().amountHits;
        }
        stats_.amountRequests = cacheSys_.front()->getStats().amountRequests;
        stats_.amountMisses = cacheSys_.back()->getStats().amountMisses;
    }

public:
    explicit CacheSystem(const cacheSystemParams& params) {
        if (params.levels.empty()) {
            throw std::invalid_argument("At least one cache level is needed");
        }

        cacheSys_.reserve(params.levels.size());

        for (const auto& description : params.levels) {
            switch (description.strategy) {
                case cacheEviction_t::C_LRU:
                    cacheSys_.push_back(
                        std::make_unique<CacheLRU<T, keyT>>(description.size, description.level));
                    break;
                case cacheEviction_t::C_LFU:
                    cacheSys_.push_back(
                        std::make_unique<CacheLFU<T, keyT>>(description.size, description.level));
                    break;
                case cacheEviction_t::C_ARC:
                    cacheSys_.push_back(
                        std::make_unique<CacheARC<T, keyT>>(description.size, description.level));
                    break;
                case cacheEviction_t::C_2Q:
                    cacheSys_.push_back(
                        std::make_unique<Cache2Q<T, keyT>>(description.size, description.level));
                    break;
                case cacheEviction_t::C_LIRS:
                    cacheSys_.push_back(
                        std::make_unique<CacheLIRS<T, keyT>>(description.size, description.level));
                    break;
                default:
                    throw std::invalid_argument("Unsupported cache strategy");
            }
        }
    }

    template <typename F>
    const T& lookupUpdate(keyT key, F slow_get_page) {
        static_assert(std::is_lvalue_reference_v<decltype(slow_get_page(key))>,
                      "The page loader must return a reference to a live page");
        try {
            const T& page = lookupUpdate(0, key, slow_get_page);
            updateStats();
            return page;
        } catch (...) {
            updateStats();
            throw;
        }
    }

    const cacheStats& getStats() const { return stats_; }

    CacheSystemStats getSystemStats() const {
        CacheSystemStats result;
        result.levels.reserve(cacheSys_.size());

        for (const auto& level : cacheSys_) {
            const auto& stats = level->getStats();
            result.levels.push_back({level->getLevel(), stats});
        }

        result.total = stats_;

        return result;
    }
};

} // namespace cache

#endif /* CACHE_SYSTEM_H */
