#pragma once

#include <cstddef>
#include <functional>
#include <optional>

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
    Cache(std::size_t size, cacheLevel_t level = cacheLevel_t::L1)
        : level_(level), size_(size) {}

    virtual ~Cache() = default;

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    cacheLevel_t getLevel() const { return level_; }
    std::size_t getSize() const { return size_; }
    const cacheStats& getStats() const { return stats_; }

    template <typename F>
    const T& lookupUpdate(keyT key, F slow_get_page) {
        ++stats_.amountRequests;

        if (auto page = getPage(key)) {
            ++stats_.amountHits;
            return page->get();
        }

        ++stats_.amountMisses;
        T& page = slow_get_page(key);

        if (size_ != 0) {
            insert(key, page);
        }

        return page;
    }

protected:
    using pageResult_t = std::optional<std::reference_wrapper<const T>>;

    // Returns a resident page reference,
    // or std::nullopt on a miss (including ghost entries).
    virtual pageResult_t getPage(const keyT& key) = 0;

    virtual void insert(const keyT& key, const T& page) = 0;
};

} // namespace cache
