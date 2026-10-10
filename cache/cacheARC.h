#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "cache.h"

namespace cache {

template <typename T, typename KeyT = int>
class CacheARC : public Cache<T, KeyT> {
private:
    enum class ListName { NO_LIST, T1, T2, B1, B2 };

    struct PageRecord {
        KeyT key;
        std::optional<T> page;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;

    struct PagePosition {
        PageIterator iterator;
        ListName listName;
    };

    using PageIndex = std::unordered_map<KeyT, PagePosition>;
    using IndexIterator = typename PageIndex::iterator;

    PageList t1_, t2_;
    PageList b1_, b2_;

    PageIndex pageIndex_;

    std::size_t capacity_;
    std::size_t t1TargetSize_ = 0;

    IndexIterator findResidentToEvict(bool requestedFromB2) {
        const bool evictFromT1 = !t1_.empty() && (t1_.size() > t1TargetSize_ ||
                                                  (requestedFromB2 && t1_.size() == t1TargetSize_));

        auto& source = (evictFromT1 || t2_.empty()) ? t1_ : t2_;
        if (source.empty()) {
            return pageIndex_.end();
        }

        return pageIndex_.find(source.back().key);
    }

    void evictPage(IndexIterator residentToEvict) noexcept {
        if (residentToEvict == pageIndex_.end()) {
            return;
        }

        auto& position = residentToEvict->second;
        const bool fromT1 = position.listName == ListName::T1;

        auto& source = fromT1 ? t1_ : t2_;
        auto& ghost = fromT1 ? b1_ : b2_;

        position.iterator->page.reset();
        ghost.splice(ghost.begin(), source, position.iterator);
        position.listName = fromT1 ? ListName::B1 : ListName::B2;
    }

    void evictForNewPage() {
        PageList* listToErase = nullptr;
        bool needsReplacement = false;

        const std::size_t recentTotal = t1_.size() + b1_.size();
        if (recentTotal == capacity_) {
            if (t1_.size() < capacity_) {
                listToErase = &b1_;
                needsReplacement = true;
            } else {
                listToErase = &t1_;
            }
        } else if (recentTotal < capacity_) {
            const std::size_t total = t1_.size() + t2_.size() + b1_.size() + b2_.size();

            if (total >= capacity_) {
                if (total >= 2 * capacity_) {
                    listToErase = &b2_;
                }
                needsReplacement = true;
            }
        }

        auto residentToEvict = needsReplacement ? findResidentToEvict(false) : pageIndex_.end();

        if (listToErase && !listToErase->empty()) {
            pageIndex_.erase(listToErase->back().key);
            listToErase->pop_back();
        }

        evictPage(residentToEvict);
    }

    const T& promoteGhostPage(IndexIterator indexedRecord, const T& page) {
        const bool wasInB2 = indexedRecord->second.listName == ListName::B2;

        auto pageIterator = indexedRecord->second.iterator;

        pageIterator->page.emplace(page);
        try {
            evictPage(findResidentToEvict(wasInB2));
        } catch (...) {
            pageIterator->page.reset();
            throw;
        }

        if (wasInB2) {
            t2_.splice(t2_.begin(), b2_, pageIterator);
        } else {
            t2_.splice(t2_.begin(), b1_, pageIterator);
        }

        indexedRecord->second.iterator = pageIterator;
        indexedRecord->second.listName = ListName::T2;
        return *pageIterator->page;
    }

    void recordHit(PagePosition& position) {
        PageIterator iterator = position.iterator;

        switch (position.listName) {
            case ListName::T1:
                assert(iterator->page.has_value());
                t2_.splice(t2_.begin(), t1_, iterator);
                position.listName = ListName::T2;
                return;
            case ListName::T2:
                assert(iterator->page.has_value());
                t2_.splice(t2_.begin(), t2_, iterator);
                return;
            case ListName::B1: {
                const std::size_t addition = std::max<std::size_t>(1, b2_.size() / b1_.size());
                t1TargetSize_ = std::min(capacity_, t1TargetSize_ + addition);
                return;
            }
            case ListName::B2: {
                const std::size_t subtrahend = std::max<std::size_t>(1, b1_.size() / b2_.size());

                t1TargetSize_ = (subtrahend >= t1TargetSize_) ? 0 : t1TargetSize_ - subtrahend;
                return;
            }
            default:
                assert(false && "Unexpected listName in recordHit");
                return;
        }
    }

public:
    explicit CacheARC(std::size_t capacity) : Base(capacity), capacity_(capacity) {}

protected:
    PageResult getPage(const KeyT& key) override {
        auto indexedRecord = pageIndex_.find(key);

        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }

        auto& position = indexedRecord->second;
        recordHit(position);

        const auto& storedPage = position.iterator->page;

        if (!storedPage.has_value()) {
            return std::nullopt;
        }

        return std::cref(*storedPage);
    }

    const T& insert(const KeyT& key, const T& page) override {
        auto indexedRecord = pageIndex_.find(key);

        if (indexedRecord != pageIndex_.end() && (indexedRecord->second.listName == ListName::B1 ||
                                                  indexedRecord->second.listName == ListName::B2)) {
            return promoteGhostPage(indexedRecord, page);
        }

        PageList stagedPage;
        stagedPage.push_front(PageRecord{key, std::optional<T>{page}});

        auto [indexedPage, wasInserted] =
            pageIndex_.emplace(key, PagePosition{stagedPage.begin(), ListName::T1});
        if (!wasInserted) {
            throw std::logic_error("insert requires an unknown key");
        }

        try {
            evictForNewPage();
        } catch (...) {
            pageIndex_.erase(indexedPage);
            throw;
        }

        t1_.splice(t1_.begin(), stagedPage);
        return *t1_.front().page;
    }
};

} // namespace cache
