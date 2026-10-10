#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace cache {

enum class CacheLevel { L1, L2, L3, L4, L5 };

enum class CacheEviction { C_LRU, C_LFU, C_ARC, C_LIRS, C_2Q, C_REF, C_UNKNOWN };

struct CacheDescription {
    std::size_t size;
    CacheLevel level;
    CacheEviction strategy;
};

struct CacheStats {
    std::size_t amountRequests = 0;
    std::size_t amountHits = 0;
    std::size_t amountMisses = 0;
};

template <typename T, typename KeyT = int>
class Cache {
private:
    CacheLevel level_;
    std::size_t size_;

    CacheStats stats_;

public:
    Cache(std::size_t size, CacheLevel level = CacheLevel::L1) : level_(level), size_(size) {
        if (size == 0) {
            throw std::invalid_argument("Cache capacity must be positive");
        }
    }

    virtual ~Cache() = default;

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    CacheLevel getLevel() const { return level_; }
    std::size_t getSize() const { return size_; }
    const CacheStats& getStats() const { return stats_; }

    template <typename F>
    const T& lookupUpdate(KeyT key, F slowGetPage) {
        using LoaderResult = decltype(slowGetPage(key));
        static_assert(std::is_same_v<LoaderResult, T&> || std::is_same_v<LoaderResult, const T&>,
                      "The loader must return T& or const T&");

        ++stats_.amountRequests;

        if (auto page = getPage(key)) {
            ++stats_.amountHits;
            return page->get();
        }

        ++stats_.amountMisses;
        const T& page = slowGetPage(key);

        return insert(key, page);
    }

protected:
    using PageResult = std::optional<std::reference_wrapper<const T>>;

    // Returns a resident page reference,
    // or std::nullopt on a miss (including ghost entries).
    virtual PageResult getPage(const KeyT& key) = 0;

    // Returns the stored copy, valid until its eviction or cache destruction.
    virtual const T& insert(const KeyT& key, const T& page) = 0;
};

} // namespace cache
