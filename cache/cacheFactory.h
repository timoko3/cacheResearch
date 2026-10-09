#pragma once

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "cache.h"
#include "cache2Q.h"
#include "cacheARC.h"
#include "cacheLFU.h"
#include "cacheLIRS.h"
#include "cacheLRU.h"

namespace cache {

template <typename T, typename KeyT = int>
class CacheFactory {
private:
    using Base = Cache<T, KeyT>;
    using Creator = std::unique_ptr<Base> (*)(const CacheDescription&);

    template <template <typename, typename> class Strategy>
    static std::unique_ptr<Base> create(const CacheDescription& description) {
        return std::make_unique<Strategy<T, KeyT>>(description.size, description.level);
    }

public:
    static std::unique_ptr<Base> make(const CacheDescription& description) {
        static const std::unordered_map<CacheEviction, Creator> creators{
            {CacheEviction::C_LRU, &create<CacheLRU>},
            {CacheEviction::C_LFU, &create<CacheLFU>},
            {CacheEviction::C_ARC, &create<CacheARC>},
            {CacheEviction::C_2Q, &create<Cache2Q>},
            {CacheEviction::C_LIRS, &create<CacheLIRS>},
        };

        auto creator = creators.find(description.strategy);
        if (creator == creators.end())

        {
            throw std::invalid_argument("Unsupported cache strategy");
        }
        return creator->second(description);
    }
};

} // namespace cache
