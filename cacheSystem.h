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

template <typename T, typename keyT = int>
class CacheSystem {
    std::vector<std::unique_ptr<Cache<T, keyT>>> cacheSys_;

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
                    throw std::invalid_argument("2Q requires KIn and KOut parameters");
                case C_LIRS:
                    throw std::invalid_argument("LIRS requires a hirSize parameter");
                default:
                    throw std::invalid_argument("Unsupported cache strategy");
            }
        }
    }
};

} // namespace cache

#endif /* CACHE_SYSTEM_H */
