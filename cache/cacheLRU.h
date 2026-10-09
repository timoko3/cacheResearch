#pragma once

#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheLRU : public Cache<T, keyT> {
private:
    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;

    struct PageRecord {
        keyT key;
        T page;

        PageRecord(const keyT& pageKey, const T& pageValue) : key(pageKey), page(pageValue) {}
    };

    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;
    using PageIndex = std::unordered_map<keyT, PageIterator>;
    using IndexIterator = typename PageIndex::iterator;

    static_assert(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<keyT>,
                  "Pages and keys must have non-throwing destructors");

    PageList cache_;
    PageIndex pageIndex_;

    void recordHit(PageIterator resident) noexcept {
        cache_.splice(cache_.begin(), cache_, resident);
    }

    void evictPage(IndexIterator indexedPage) noexcept {
        auto resident = indexedPage->second;
        pageIndex_.erase(indexedPage);
        cache_.erase(resident);
    }

    void insertNewPage(const keyT& key, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(key, page);
        auto [indexedPage, wasInserted] = pageIndex_.emplace(key, stagedPage.begin());
        if (!wasInserted) {
            throw std::logic_error("insertNewPage requires an unknown key");
        }

        // Find the victim after emplace: rehashing invalidates index iterators.
        auto residentToEvict = pageIndex_.end();
        try {
            if (isFull()) {
                residentToEvict = pageIndex_.find(cache_.back().key);
            }
        } catch (...) {
            pageIndex_.erase(indexedPage);
            throw;
        }

        if (residentToEvict != pageIndex_.end()) {
            evictPage(residentToEvict);
        }
        cache_.splice(cache_.begin(), stagedPage, stagedPage.begin());
    }

public:
    explicit CacheLRU(std::size_t capacity, cacheLevel_t level = cacheLevel_t::L1)
        : Base(capacity, level) {}

    std::size_t getResidentCount() const noexcept { return cache_.size(); }
    std::size_t getIndexedCount() const noexcept { return pageIndex_.size(); }
    const PageList& getCache() const noexcept { return cache_; }
    bool isFull() const noexcept { return getResidentCount() >= this->getSize(); }

protected:
    pageResult_t getPage(const keyT& key) override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }

        recordHit(indexedRecord->second);
        return std::cref(indexedRecord->second->page);
    }

    void insert(const keyT& key, const T& page) override { insertNewPage(key, page); }
};

} // namespace cache
