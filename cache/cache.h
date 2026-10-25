#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace cache {

struct CacheStats {
    std::size_t amountRequests = 0;
    std::size_t amountHits = 0;
    std::size_t amountMisses = 0;
};

template <typename T, typename KeyT = int>
class Cache {
private:
    std::size_t size_;

    CacheStats stats_;

public:
    using PageResult = std::optional<std::reference_wrapper<const T>>;

    explicit Cache(std::size_t size) : size_(size) {
        if (size == 0) {
            throw std::invalid_argument("Cache capacity must be positive");
        }
    }

    virtual ~Cache() = default;

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    std::size_t getSize() const { return size_; }
    const CacheStats& getStats() const { return stats_; }

    // Does not change cache state or statistics; ghosts count as misses.
    // The reference is valid until eviction or cache destruction.
    virtual PageResult lookup(const KeyT& key) const = 0;

    // Keeps an existing page unchanged; otherwise stores a copy with eviction.
    // Does not update statistics. The reference is valid until eviction.
    const T& insert(const KeyT& key, const T& page) {
        if (auto resident = lookup(key)) {
            return resident->get();
        }
        return doInsert(key, page);
    }

    template <typename F>
    const T& lookupUpdate(KeyT key, F slowGetPage) {
        using LoaderResult = decltype(slowGetPage(key));
        static_assert(std::is_same_v<LoaderResult, T&> ||
                          std::is_same_v<LoaderResult, const T&>,
                      "The loader must return T& or const T&");

        ++stats_.amountRequests;

        if (auto page = getPage(key)) {
            ++stats_.amountHits;
            return page->get();
        }

        ++stats_.amountMisses;
        const T& page = slowGetPage(key);

        return doInsert(key, page);
    }

protected:
    // Returns a resident page reference,
    // or std::nullopt on a miss (including ghost entries).
    virtual PageResult getPage(const KeyT& key) = 0;

    // Returns the stored copy, valid until its eviction or cache destruction.
    virtual const T& doInsert(const KeyT& key, const T& page) = 0;
};

} // namespace cache
