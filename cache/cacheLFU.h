#pragma once

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheLFU : public Cache<T, keyT> {
private:
    struct Entry_t {
        keyT key;
        T page;
        std::size_t frequency = 0;
    };

    using Base_t = Cache<T, keyT>;
    using List_t = std::list<Entry_t>;
    using ListIt_t = typename List_t::iterator;

    std::unordered_map<keyT, ListIt_t> hash_;
    List_t cache_;

    static bool compareEntriesByFreq(const Entry_t& src1, const Entry_t& src2) {
        return src1.frequency < src2.frequency;
    }

    bool isFull() const { return cache_.size() >= this->getSize(); }

public:
    explicit CacheLFU(size_t size, cacheLevel level = L1) : Base_t(size, level){};

    ~CacheLFU() = default;

protected:
    const T* findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        auto entryIt = hit->second;
        entryIt->frequency++;
        cache_.splice(cache_.end(), cache_, entryIt);
        return std::addressof(entryIt->page);
    }

    void insert(const keyT& key, T page) override {
        if (isFull()) {
            auto victim = std::min_element(cache_.begin(), cache_.end(), compareEntriesByFreq);
            hash_.erase(victim->key);
            cache_.erase(victim);
        }

        Entry_t new_entry = {key, page, 1};
        cache_.push_back(new_entry);
        hash_.emplace(key, std::prev(cache_.end()));
    }
};

} // namespace cache
