#pragma once

#include <cstddef>

namespace cache {

// Keep levels consecutive from zero and add new levels before COUNT.
// COUNT is a sentinel, not a usable cache level.
enum class CacheLevel { L1 = 0, L2, L3, L4, L5, COUNT };

inline constexpr std::size_t maxCacheLevel = static_cast<std::size_t>(CacheLevel::COUNT);

enum class CacheEviction { C_LRU, C_LFU, C_ARC, C_LIRS, C_2Q, C_REF, C_UNKNOWN };

struct CacheDescription {
    std::size_t size;
    CacheLevel level;
    CacheEviction strategy;
};

} // namespace cache
