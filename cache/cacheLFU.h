#pragma once

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename KeyT = int>
class CacheLFU : public Cache<T, KeyT> {
private:
    struct PageRecord {
        KeyT key;
        T page;
        std::size_t frequency = 0;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;
    using PageIndex = std::unordered_map<KeyT, PageIterator>;

    PageIndex pageIndex_;
    PageList cache_;

    static bool comparePageRecordsByFrequency(const PageRecord& first,
                                              const PageRecord& second) {
        return first.frequency < second.frequency;
    }

    bool isFull() const { return cache_.size() >= this->getSize(); }

    void recordHit(PageIterator resident) {
        resident->frequency++;
        cache_.splice(cache_.end(), cache_, resident);
    }

public:
    explicit CacheLFU(std::size_t capacity) : Base(capacity){};

    ~CacheLFU() = default;

    PageResult lookup(const KeyT& key) const override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }
        return std::cref(indexedRecord->second->page);
    }

protected:
    PageResult getPage(const KeyT& key) override {
        auto indexedRecord = pageIndex_.find(key);

        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }

        auto resident = indexedRecord->second;
        recordHit(resident);

        return std::cref(resident->page);
    }

    const T& doInsert(const KeyT& key, const T& page) override {
        PageList stagedPage;

        constexpr std::size_t initialFrequency = 1;
        stagedPage.push_back(PageRecord{key, page, initialFrequency});

        auto [indexedPage, wasInserted] =
            pageIndex_.emplace(key, stagedPage.begin());
        if (!wasInserted) {
            throw std::logic_error("insert requires an unknown key");
        }

        try {
            if (isFull()) {
                auto residentToEvict = std::min_element(
                    cache_.begin(), cache_.end(), comparePageRecordsByFrequency);

                pageIndex_.erase(residentToEvict->key);
                cache_.erase(residentToEvict);
            }
        } catch (...) {
            pageIndex_.erase(indexedPage);
            throw;
        }

        cache_.splice(cache_.end(), stagedPage);
        return cache_.back().page;
    }
};

} // namespace cache
