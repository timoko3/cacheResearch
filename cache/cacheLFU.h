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

    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;
    using List_t = std::list<Entry_t>;
    using ListIt_t = typename List_t::iterator;
    using HashIt_t = typename std::unordered_map<keyT, ListIt_t>::iterator;

    std::unordered_map<keyT, ListIt_t> hash_;
    List_t cache_;

    static bool compareEntriesByFreq(const Entry_t& src1, const Entry_t& src2) {
        return src1.frequency < src2.frequency;
    }

    bool isFull() const { return cache_.size() >= this->getSize(); }

    void recordHit(ListIt_t hitIt) {
        hitIt->frequency++;
        cache_.splice(cache_.end(), cache_, hitIt);
    }

public:
    CacheLFU(std::size_t size, cacheLevel_t level = cacheLevel_t::L1)
        : Base(size, level) {};

    ~CacheLFU() = default;

protected:
    pageResult_t getPage(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return std::nullopt;
        }

        auto hitIt = hit->second;
        recordHit(hitIt);

        return std::cref(hitIt->page);
    }

    void insert(const keyT& key, const T& page) override {
        auto victim = cache_.end();
        if (isFull()) {
            victim = std::min_element(cache_.begin(), cache_.end(), compareEntriesByFreq);
        }

        constexpr std::size_t kInitialFrequency = 1;
        cache_.push_back(Entry_t{key, page, kInitialFrequency});

        HashIt_t hashIt;
        bool emplaced = false;

        try {
            auto emplaceResult = hash_.emplace(key, std::prev(cache_.end()));
            if (!emplaceResult.second) {
                throw std::logic_error("Duplicate key");
            }

            hashIt = emplaceResult.first;
            emplaced = true;

            if (victim != cache_.end()) {
                hash_.erase(victim->key);
                cache_.erase(victim);
            }
        } catch (...) {
            if (emplaced) {
                hash_.erase(hashIt);
            }
            cache_.pop_back();
            throw;
        }
    }
};

} // namespace cache
