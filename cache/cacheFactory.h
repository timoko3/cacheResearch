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

template <typename T, typename keyT = int>
class CacheFactory {
private:
    using Base = Cache<T, keyT>;
    using Creator = std::unique_ptr<Base> (*)(const cacheDescription&);

    template <template <typename, typename> class Strategy>
    static std::unique_ptr<Base> create(const cacheDescription& description) {
        return std::make_unique<Strategy<T, keyT>>(description.size, description.level);
    }

public:
    static std::unique_ptr<Base> make(const cacheDescription& description) {
        static const std::unordered_map<cacheEviction_t, Creator> creators{
            {cacheEviction_t::C_LRU, &create<CacheLRU>},
            {cacheEviction_t::C_LFU, &create<CacheLFU>},
            {cacheEviction_t::C_ARC, &create<CacheARC>},
            {cacheEviction_t::C_2Q, &create<Cache2Q>},
            {cacheEviction_t::C_LIRS, &create<CacheLIRS>},
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
