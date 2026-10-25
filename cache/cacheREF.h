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
    struct PageRecord {
        KeyT key;
        T page;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using PageList = std::list<PageRecord>;
    using RequestList = std::list<KeyT>;
    using PageIterator = typename PageList::iterator;
    using RequestIterator = typename RequestList::iterator;
    using PageIndex = std::unordered_map<KeyT, PageIterator>;
    using Distance = typename RequestList::difference_type;

    PageIndex pageIndex_;
    PageList cache_;
    RequestList requests_;
    RequestIterator currentRequest_;

    bool isFull() const { return cache_.size() >= this->getSize(); };

    PageIterator findResidentToEvict() {
        PageIterator residentToEvict = cache_.begin();
        Distance maxDistance = 0;

        for (auto pageIterator = cache_.begin(); pageIterator != cache_.end();
             ++pageIterator) {
            Distance distanceToNextRequest = requests_.size();

            for (auto requestIterator = currentRequest_;
                 requestIterator != requests_.end();
                 ++requestIterator) {
                if (*requestIterator == pageIterator->key) {
                    distanceToNextRequest =
                        std::distance(currentRequest_, requestIterator);
                    break;
                }
            }

            if (distanceToNextRequest >= maxDistance) {
                maxDistance = distanceToNextRequest;
                residentToEvict = pageIterator;
            }
        }

        return residentToEvict;
    };

public:
    CacheREF(std::size_t capacity, RequestList requests)
        : Base(capacity), requests_(requests), currentRequest_(requests_.begin()) {
        if (requests_.empty()) {
            throw std::invalid_argument("Array with requests are empty");
        }
    };

    ~CacheREF() = default;

    PageResult lookup(const KeyT& key) const override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }
        return std::cref(indexedRecord->second->page);
    }

protected:
    virtual PageResult getPage(const KeyT& key) override {
        if (currentRequest_ == requests_.end()) {
            throw std::out_of_range("Request sequence exhausted");
        }

        if (*currentRequest_ != key) {
            throw std::invalid_argument(
                "Request does not match the supplied sequence");
        }

        ++currentRequest_;

        auto indexedRecord = pageIndex_.find(key);

        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }

        return std::cref(indexedRecord->second->page);
    }

    const T& doInsert(const KeyT& key, const T& page) override {
        PageList stagedPage;
        stagedPage.push_front({key, page});

        auto [indexedPage, wasInserted] =
            pageIndex_.emplace(key, stagedPage.begin());
        if (!wasInserted) {
            throw std::logic_error("insert requires an unknown key");
        }

        try {
            if (isFull()) {
                auto residentToEvict = findResidentToEvict();
                pageIndex_.erase(residentToEvict->key);
                cache_.erase(residentToEvict);
            }
        } catch (...) {
            pageIndex_.erase(indexedPage);
            throw;
        }

        cache_.splice(cache_.begin(), stagedPage);
        return cache_.front().page;
    }
};

} // namespace cache
