#pragma once

#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename KeyT = int>
class CacheREF : public Cache<T, KeyT> {
private:
    struct Entry {
        KeyT key;
        T page;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using CacheList = std::list<Entry>;
    using RequestList = std::list<KeyT>;
    using CacheIterator = typename CacheList::iterator;
    using RequestIterator = typename RequestList::iterator;
    using HashTable = std::unordered_map<KeyT, CacheIterator>;
    using Distance = typename RequestList::difference_type;

    HashTable hash_;
    CacheList cache_;
    RequestList requests_;
    RequestIterator curRec_;

    bool isFull() const { return cache_.size() >= this->getSize(); };

    CacheIterator findVictim() {
        CacheIterator victimIt = cache_.begin();
        Distance maxDist = 0;

        for (auto cacheIt = cache_.begin(); cacheIt != cache_.end(); ++cacheIt) {
            Distance distToNext = requests_.size();

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
    CacheREF(std::size_t size, RequestList reqs, CacheLevel level = CacheLevel::L1)
        : Base(size, level), requests_(reqs), curRec_(requests_.begin()) {
        if (requests_.empty()) {
            throw std::invalid_argument("Array with requests are empty");
        }
    };

    ~CacheREF() = default;

protected:
    virtual PageResult getPage(const KeyT& key) override {
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

    const T& insert(const KeyT& key, const T& page) override {
        CacheList stagedPage;
        stagedPage.push_front({key, page});

        auto [indexedPageIt, wasIndexed] = hash_.emplace(key, stagedPage.begin());
        if (!wasIndexed) {
            throw std::logic_error("insert requires an unknown key");
        }

        try {
            if (isFull()) {
                auto victimIt = findVictim();
                hash_.erase(victimIt->key);
                cache_.erase(victimIt);
            }
        } catch (...) {
            hash_.erase(indexedPageIt);
            throw;
        }

        cache_.splice(cache_.begin(), stagedPage);
        return cache_.front().page;
    }
};

} // namespace cache
