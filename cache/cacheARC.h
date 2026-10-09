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

template <typename T, typename keyT = int>
class CacheARC : public Cache<T, keyT> {
private:
    enum class listName_t { NO_LIST, T1, T2, B1, B2 };

    struct entry_t {
        keyT key;
        std::optional<T> page;
    };

    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;
    using list_t = std::list<entry_t>;
    using listIt_t = typename list_t::iterator;

    struct pageLoc_t {
        listIt_t listIt;
        listName_t listName;
    };

    using hash_t = std::unordered_map<keyT, pageLoc_t>;
    using hashIt_t = typename hash_t::iterator;

    list_t T1_, T2_;
    list_t B1_, B2_;

    hash_t hash_;

    std::size_t capacity_;
    std::size_t targetT1size_ = 0;

    void eraseLRU(list_t& list) {
        if (list.empty()) {
            return;
        }

        auto victim = std::prev(list.end());

        hash_.erase(victim->key);
        list.erase(victim);
    }

    void moveLRUToGhost(list_t& srcList, list_t& ghostList, listName_t ghostListName) {
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
        const bool evictFromT1 = !T1_.empty() && (T1_.size() > targetT1size_ ||
                                                  (requestedFromB2 && T1_.size() == targetT1size_));

        if (evictFromT1 || T2_.empty()) {
            moveLRUToGhost(T1_, B1_, listName_t::B1);
        } else {
            moveLRUToGhost(T2_, B2_, listName_t::B2);
        }
    }

    void moveGhostPageToT2(hashIt_t hit, const T& page) {
        const bool wasInB2 = hit->second.listName == listName_t::B2;

        auto pageIt = hit->second.listIt;

        replacePage(wasInB2);
        pageIt->page.emplace(page);

        if (wasInB2) {
            T2_.splice(T2_.begin(), B2_, pageIt);
        } else {
            T2_.splice(T2_.begin(), B1_, pageIt);
        }

        hit->second.listIt = pageIt;
        hit->second.listName = listName_t::T2;
    }

    void recordHit(pageLoc_t& pageLoc) {
        listIt_t listIt = pageLoc.listIt;

        switch (pageLoc.listName) {
            case listName_t::T1:
                assert(listIt->page.has_value());
                T2_.splice(T2_.begin(), T1_, listIt);
                pageLoc.listName = listName_t::T2;
                return;
            case listName_t::T2:
                assert(listIt->page.has_value());
                T2_.splice(T2_.begin(), T2_, listIt);
                return;
            case listName_t::B1: {
                const std::size_t addition = std::max<std::size_t>(1, B2_.size() / B1_.size());
                targetT1size_ = std::min(capacity_, targetT1size_ + addition);
                return;
            }
            case listName_t::B2: {
                const std::size_t subtrahend = std::max<std::size_t>(1, B1_.size() / B2_.size());

                targetT1size_ = (subtrahend >= targetT1size_) ? 0 : targetT1size_ - subtrahend;
                return;
            }
            default:
                assert(false && "Unexpected listName in recordHit");
                return;
        }
    }

public:
    CacheARC(std::size_t capacity, cacheLevel_t level = cacheLevel_t::L1)
        : Base(capacity, level), capacity_(capacity) {}

protected:
    pageResult_t getPage(const keyT& key) override {
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

    void insert(const keyT& key, const T& page) override {
        auto hit = hash_.find(key);

        if (hit != hash_.end() &&
            (hit->second.listName == listName_t::B1 || hit->second.listName == listName_t::B2)) {
            moveGhostPageToT2(hit, page);
            return;
        }

        const std::size_t recentTotal = T1_.size() + B1_.size();

        if (recentTotal == capacity_) {
            if (T1_.size() < capacity_) {
                eraseLRU(B1_);
                replacePage(false);
            } else {
                eraseLRU(T1_);
            }
        } else if (recentTotal < capacity_) {
            const std::size_t total = T1_.size() + T2_.size() + B1_.size() + B2_.size();

            if (total >= capacity_) {
                if (total >= 2 * capacity_) {
                    eraseLRU(B2_);
                }
                replacePage(false);
            }
        }

        T1_.push_front(entry_t{key, std::optional<T>{page}});
        hash_.emplace(key, pageLoc_t{T1_.begin(), listName_t::T1});
    }
};

} // namespace cache
