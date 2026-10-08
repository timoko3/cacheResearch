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

    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;
    using CacheList_t = std::list<Entry_t>;
    using ReqList_t = std::list<keyT>;
    using CacheIt_t = typename CacheList_t::iterator;
    using ReqIt_t = typename ReqList_t::iterator;
    using HashTable_t = std::unordered_map<keyT, CacheIt_t>;
    using Dist_t = typename ReqList_t::difference_type;

    HashTable_t hash_;
    CacheList_t cache_;
    ReqList_t requests_;
    ReqIt_t curRec_;

    bool isFull() const { return cache_.size() >= this->getSize(); };

    CacheIt_t findVictim() {
        CacheIt_t victimIt = cache_.begin();
        Dist_t maxDist = 0;

        for (auto cacheIt = cache_.begin(); cacheIt != cache_.end(); ++cacheIt) {
            Dist_t distToNext = requests_.size();

            for (auto reqIt = curRec_; reqIt != requests_.end(); ++reqIt) {
                if (*reqIt == cacheIt->key) {
                    distToNext = std::distance(curRec_, reqIt);
                    break;
                }
            }

            if (distToNext >= maxDist) {
                maxDist = distToNext;
                victimIt = cacheIt;
            }
        }

        return victimIt;
    };

public:
    CacheREF(std::size_t size, ReqList_t reqs, cacheLevel_t level = cacheLevel_t::L1)
        : Base(size, level), requests_(reqs), curRec_(requests_.begin()) {
        if (requests_.empty()) {
            throw std::invalid_argument("Array with requests are empty");
        }
    };

    ~CacheREF() = default;

protected:
    virtual pageResult_t getPage(const keyT& key) override {
        if (curRec_ == requests_.end()) {
            throw std::out_of_range("Request sequence exhausted");
        }

        if (*curRec_ != key) {
            throw std::invalid_argument("Request does not match the supplied sequence");
        }

        ++curRec_;

        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return std::nullopt;
        }

        return std::cref(hit->second->page);
    }

    void insert(const keyT& key, const T& page) override {
        if (isFull()) {
            auto victimIt = findVictim();
            hash_.erase(victimIt->key);
            cache_.erase(victimIt);
        }

        cache_.push_front({key, page});
        hash_.emplace(key, cache_.begin());
    }
};

} // namespace cache
