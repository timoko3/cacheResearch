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

    struct Entry {
        KeyT key;
        std::optional<T> page;
    };

    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;
    using PageList = std::list<Entry>;
    using PageIterator = typename PageList::iterator;

    struct PageLocation {
        PageIterator listIt;
        ListName listName;
    };

    using PageIndex = std::unordered_map<KeyT, PageLocation>;
    using IndexIterator = typename PageIndex::iterator;

    PageList t1_, t2_;
    PageList b1_, b2_;

    PageIndex hash_;

    std::size_t capacity_;
    std::size_t targetT1size_ = 0;

    void eraseLRU(PageList& list) {
        if (list.empty()) {
            return;
        }

        auto victim = std::prev(list.end());

        hash_.erase(victim->key);
        list.erase(victim);
    }

    void moveLRUToGhost(PageList& srcList, PageList& ghostList, ListName ghostListName) {
        if (srcList.empty()) {
            return;
        }

        auto victim = std::prev(srcList.end());

        victim->page.reset();

        ghostList.splice(ghostList.begin(), srcList, victim);

        auto& location = hash_.at(victim->key);

        location.listIt = victim;
        location.listName = ghostListName;
    }

    void replacePage(bool requestedFromB2) {
        const bool evictFromT1 = !t1_.empty() && (t1_.size() > targetT1size_ ||
                                                  (requestedFromB2 && t1_.size() == targetT1size_));

        if (evictFromT1 || t2_.empty()) {
            moveLRUToGhost(t1_, b1_, ListName::B1);
        } else {
            moveLRUToGhost(t2_, b2_, ListName::B2);
        }
    }

    const T& moveGhostPageToT2(IndexIterator hit, const T& page) {
        const bool wasInB2 = hit->second.listName == ListName::B2;

        auto pageIt = hit->second.listIt;

        replacePage(wasInB2);
        pageIt->page.emplace(page);

        if (wasInB2) {
            t2_.splice(t2_.begin(), b2_, pageIt);
        } else {
            t2_.splice(t2_.begin(), b1_, pageIt);
        }

        hit->second.listIt = pageIt;
        hit->second.listName = ListName::T2;
        return *pageIt->page;
    }

    void recordHit(PageLocation& pageLoc) {
        PageIterator listIt = pageLoc.listIt;

        switch (pageLoc.listName) {
            case ListName::T1:
                assert(listIt->page.has_value());
                t2_.splice(t2_.begin(), t1_, listIt);
                pageLoc.listName = ListName::T2;
                return;
            case ListName::T2:
                assert(listIt->page.has_value());
                t2_.splice(t2_.begin(), t2_, listIt);
                return;
            case ListName::B1: {
                const std::size_t addition = std::max<std::size_t>(1, b2_.size() / b1_.size());
                targetT1size_ = std::min(capacity_, targetT1size_ + addition);
                return;
            }
            case ListName::B2: {
                const std::size_t subtrahend = std::max<std::size_t>(1, b1_.size() / b2_.size());

                targetT1size_ = (subtrahend >= targetT1size_) ? 0 : targetT1size_ - subtrahend;
                return;
            }
            default:
                assert(false && "Unexpected listName in recordHit");
                return;
        }
    }

public:
    CacheARC(std::size_t capacity, CacheLevel level = CacheLevel::L1)
        : Base(capacity, level), capacity_(capacity) {}

protected:
    PageResult getPage(const KeyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return std::nullopt;
        }

        auto& pageLoc = hit->second;
        recordHit(pageLoc);

        const auto& storedPage = pageLoc.listIt->page;

        if (!storedPage.has_value()) {
            return std::nullopt;
        }

        return std::cref(*storedPage);
    }

    const T& insert(const KeyT& key, const T& page) override {
        auto hit = hash_.find(key);

        if (hit != hash_.end() &&
            (hit->second.listName == ListName::B1 || hit->second.listName == ListName::B2)) {
            return moveGhostPageToT2(hit, page);
        }

        const std::size_t recentTotal = t1_.size() + b1_.size();

        if (recentTotal == capacity_) {
            if (t1_.size() < capacity_) {
                eraseLRU(b1_);
                replacePage(false);
            } else {
                eraseLRU(t1_);
            }
        } else if (recentTotal < capacity_) {
            const std::size_t total = t1_.size() + t2_.size() + b1_.size() + b2_.size();

            if (total >= capacity_) {
                if (total >= 2 * capacity_) {
                    eraseLRU(b2_);
                }
                replacePage(false);
            }
        }

        t1_.push_front(Entry{key, std::optional<T>{page}});
        hash_.emplace(key, PageLocation{t1_.begin(), ListName::T1});
        return *t1_.front().page;
    }
};

} // namespace cache
