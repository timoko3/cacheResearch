#pragma once

#include <iterator>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheLRU : public Cache<T, keyT> {
private:
    using Base = Cache<T, keyT>;
    using Entry = std::pair<keyT, T>;
    using List = std::list<Entry>;
    using ListIt = typename List::iterator;

    List cache_;
    std::unordered_map<keyT, ListIt> hash_;

public:
    explicit CacheLRU(size_t size, cacheLevel_t level = L1) : Base(size, level) {}

    const List& getCache() const { return cache_; }

    const std::unordered_map<keyT, ListIt>& getHash() const { return hash_; }

    bool isFull() const { return cache_.size() >= this->getSize(); }

protected:
    const T* findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        cache_.splice(cache_.begin(), cache_, hit->second);
        return std::addressof(hit->second->second);
    }

    void insert(const keyT& key, T page) override {
        if (isFull()) {
            hash_.erase(cache_.back().first);
            cache_.pop_back();
        }

        cache_.emplace_front(key, std::move(page));
        hash_.emplace(key, cache_.begin());
    }
};

} // namespace cache
