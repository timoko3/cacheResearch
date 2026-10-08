#pragma once

#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheREF : public Cache<T, keyT> {
private:
    struct Entry_t {
        keyT key;
        T page;
    };

    using Base_t = Cache<T, keyT>;
    using CacheList_t = std::list<Entry_t>;
    using ReqList_t = std::list<keyT>;
    using CacheIt_t = typename CacheList_t::iterator;
    using ReqIt_t = typename ReqList_t::iterator;
    using HashTable_t = std::unordered_map<keyT, CacheIt_t>;
    using Dist_t = typename ReqList_t::difference_type;

    HashTable_t hash_;
    CacheList_t cache_;
    ReqList_t requests_;
    ReqIt_t pos_;

    bool isFull() { return cache_.size() >= this->getSize(); };

    CacheIt_t findVictim() {
        CacheIt_t victim_it = cache_.begin();
        Dist_t max_dist = 0;

        for (auto cache_it = cache_.begin(); cache_it != cache_.end(); ++cache_it) {
            Dist_t dist_to_next = requests_.size();

            for (auto req_it = pos_; req_it != requests_.end(); ++req_it) {
                if (*req_it == cache_it->key) {
                    dist_to_next = std::distance(pos_, req_it);
                    break;
                }
            }

            if (dist_to_next >= max_dist) {
                max_dist = dist_to_next;
                victim_it = cache_it;
            }
        }

        return victim_it;
    };

public:
    explicit CacheREF(size_t size, ReqList_t reqs, cacheLevel_t level = L1)
        : Base_t(size, level), requests_(reqs), pos_(requests_.begin()) {
        if (requests_.empty()) {
            throw std::invalid_argument("Array with requests are empty");
        }
    };

    ~CacheREF() = default;

protected:
    virtual const T* findAndTouch(const keyT& key) override {
        if (pos_ == requests_.end()) {
            throw std::out_of_range("Request sequence exhausted");
        }
        ++pos_;

        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        return std::addressof(hit->second->page);
    }

    void insert(const keyT& key, T page) override {
        if (isFull()) {
            auto victim_it = findVictim();
            hash_.erase(victim_it->key);
            cache_.erase(victim_it);
        }

        cache_.push_front({key, page});
        hash_.emplace(key, cache_.begin());
    }
};

} // namespace cache
