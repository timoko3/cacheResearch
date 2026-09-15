#ifndef CACHE_SYSTEM_H
#define CACHE_SYSTEM_H

#include <memory>
#include <stdexcept>
#include <vector>

#include "cache.h"

namespace cache {

struct cacheSystemParams {
    // Descriptions are ordered from the first cache level to the last.
    std::vector<cacheDescription> levels;
};

struct CacheLevelStats {
    cacheLevel level;
    CacheStats stats;
};

struct CacheSystemStats {
    CacheStats total;
    std::vector<CacheLevelStats> levels;
};

template <typename T, typename keyT = int>
class CacheSystem {
    std::vector<std::unique_ptr<Cache<T, keyT>>> cacheSys_;

    template <typename F>
    T lookupAt(size_t index, keyT key, F& slow_get_page){
        if(index == cacheSys_.size()){
            return slow_get_page(key);
        }

        return cacheSys_[index]->lookupUpdate(
            key, 
            [this, index, &slow_get_page](keyT requestedKey)->T{
                return lookupAt(index + 1, requestedKey, slow_get_page);
            }
        );
    }
public:
    explicit CacheSystem(const cacheSystemParams& params) {
        if (params.levels.empty()) {
            throw std::invalid_argument("At least one cache level is needed");
        }

        cacheSys_.reserve(params.levels.size());

        for (const auto& description : params.levels) {
            switch (description.strategy) {
                case C_LRU:
                    cacheSys_.push_back(std::make_unique<CacheLRU<T, keyT>>(
                        description.size, description.level));
                    break;
                case C_LFU:
                    cacheSys_.push_back(std::make_unique<CacheLFU<T, keyT>>(
                        description.size, description.level));
                    break;
                case C_ARC:
                    cacheSys_.push_back(std::make_unique<CacheARC<T, keyT>>(
                        description.size, description.level));
                    break;
                case C_2Q:
                    cacheSys_.push_back(std::make_unique<Cache2Q<T, keyT>>(
                        description.size, description.level));
                    break;
                case C_LIRS:
                    cacheSys_.push_back(std::make_unique<CacheLIRS<T, keyT>>(
                        description.size, description.level));
                    break;
                default:
                    throw std::invalid_argument("Unsupported cache strategy");
            }
        }
    }

    template <typename F>
    T lookupUpdate(keyT key, F slow_get_page){
        return lookupAt(0, key, slow_get_page);
    }

    CacheSystemStats getStats() const {
        CacheSystemStats result;
        result.levels.reserve(cacheSys_.size());

        for (const auto& level : cacheSys_) {
            const auto& stats = level->getStats();
            result.levels.push_back({level->getLevel(), stats});
            result.total.amountHits += stats.amountHits;
        }

        if (!cacheSys_.empty()) {
            result.total.amountRequests = cacheSys_.front()->getStats().amountRequests;
            result.total.amountMisses = cacheSys_.back()->getStats().amountMisses;
        }

        return result;
    }
};

} // namespace cache

#endif /* CACHE_SYSTEM_H */
