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
    struct Entry {
        KeyT key;
        T page;
        std::size_t frequency = 0;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using PageList = std::list<Entry>;
    using PageIterator = typename PageList::iterator;

    std::unordered_map<KeyT, PageIterator> hash_;
    PageList cache_;

    static bool compareEntriesByFreq(const Entry& src1, const Entry& src2) {
        return src1.frequency < src2.frequency;
    }

    bool isFull() const { return cache_.size() >= this->getSize(); }

    void recordHit(PageIterator hitIt) {
        hitIt->frequency++;
        cache_.splice(cache_.end(), cache_, hitIt);
    }

public:
    CacheLFU(std::size_t size, CacheLevel level = CacheLevel::L1) : Base(size, level){};

    ~CacheLFU() = default;

protected:
    PageResult getPage(const KeyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return std::nullopt;
        }

        auto hitIt = hit->second;
        recordHit(hitIt);

        return std::cref(hitIt->page);
    }

    const T& insert(const KeyT& key, const T& page) override {
        if (isFull()) {
            auto victim = std::min_element(cache_.begin(), cache_.end(), compareEntriesByFreq);
            hash_.erase(victim->key);
            cache_.erase(victim);
        }

        constexpr std::size_t initialFrequency = 1;
        Entry newEntry = {key, page, initialFrequency};
        cache_.push_back(newEntry);
        hash_.emplace(key, std::prev(cache_.end()));
        return cache_.back().page;
    }
};

} // namespace cache
