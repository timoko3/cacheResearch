#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace cache {

enum class cacheLevel_t { L1, L2, L3, L4, L5 };

enum class cacheEviction_t { C_LRU, C_LFU, C_ARC, C_LIRS, C_2Q, C_REF, C_UNKNOWN };

struct cacheDescription {
    std::size_t size;
    cacheLevel_t level;
    cacheEviction_t strategy;
};

struct cacheStats {
    std::size_t amountRequests = 0;
    std::size_t amountHits = 0;
    std::size_t amountMisses = 0;
};

template <typename T, typename keyT = int>
class Cache {
private:
    cacheLevel_t level_;
    std::size_t size_;

    cacheStats stats_;

public:
    Cache(std::size_t size, cacheLevel_t level = cacheLevel_t::L1) : level_(level), size_(size) {
        if (size == 0) {
            throw std::invalid_argument("Cache capacity must be positive");
        }
    }

    virtual ~Cache() = default;

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    cacheLevel_t getLevel() const { return level_; }
    std::size_t getSize() const { return size_; }
    const cacheStats& getStats() const { return stats_; }

    template <typename F>
    const T& lookupUpdate(keyT key, F slow_get_page) {
        using LoaderResult = decltype(slow_get_page(key));
        static_assert(std::is_same_v<LoaderResult, T&> ||
                          std::is_same_v<LoaderResult, const T&>,
                      "The loader must return T& or const T&");

        ++stats_.amountRequests;

        if (auto page = getPage(key)) {
            ++stats_.amountHits;
            return page->get();
        }

        ++stats_.amountMisses;
        const T& page = slow_get_page(key);

        return insert(key, page);
    }

protected:
    using pageResult_t = std::optional<std::reference_wrapper<const T>>;

    // Returns a resident page reference,
    // or std::nullopt on a miss (including ghost entries).
    virtual pageResult_t getPage(const keyT& key) = 0;

    // Returns the stored copy, valid until its eviction or cache destruction.
    virtual const T& insert(const keyT& key, const T& page) = 0;
};

} // namespace cache
