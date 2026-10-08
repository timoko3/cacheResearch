#pragma once

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheARC : public Cache<T, keyT> {
private:
    enum listName_t { NO_LIST, T1, T2, B1, B2 };

    using Base = Cache<T, keyT>;

    struct entry_t {
        keyT key;
        std::optional<T> value;
    };

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

    size_t capacity_;
    size_t targetT1size_ = 0;

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

        victim->value.reset();

        ghostList.splice(ghostList.begin(), srcList, victim);

        auto& location = hash_.at(victim->key);

        location.listIt = victim;
        location.listName = ghostListName;
    }

    void replacePage(bool requestedFromB2) {
        const bool evictFromT1 = !T1_.empty() && (T1_.size() > targetT1size_ ||
                                                  (requestedFromB2 && T1_.size() == targetT1size_));

        if (evictFromT1 || T2_.empty()) {
            moveLRUToGhost(T1_, B1_, B1);
        } else {
            moveLRUToGhost(T2_, B2_, B2);
        }
    }

    void moveGhostPageToT2(hashIt_t hit, T page) {
        const bool wasInB2 = hit->second.listName == B2;

        auto pageIt = hit->second.listIt;

        replacePage(wasInB2);
        pageIt->value.emplace(std::move(page));

        if (wasInB2) {
            T2_.splice(T2_.begin(), B2_, pageIt);
        } else {
            T2_.splice(T2_.begin(), B1_, pageIt);
        }

        hit->second.listIt = pageIt;
        hit->second.listName = T2;
    }

public:
    explicit CacheARC(size_t capacity, cacheLevel level = L1)
        : Base(capacity, level), capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("ARC cache capacity must be greater than zero");
        }
    }

protected:
    const T* findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        auto& location = hit->second;
        auto current = location.listIt;

        switch (location.listName) {
            case T1:
                T2_.splice(T2_.begin(), T1_, current);
                location.listIt = current;
                location.listName = T2;
                return std::addressof(*current->value);

            case T2:
                T2_.splice(T2_.begin(), T2_, current);
                location.listIt = current;
                return std::addressof(*current->value);

            case B1: {
                const size_t addition = std::max<size_t>(1, B2_.size() / B1_.size());
                targetT1size_ = std::min(capacity_, targetT1size_ + addition);
                return nullptr;
            }

            case B2: {
                const size_t subtrahend = std::max<size_t>(1, B1_.size() / B2_.size());
                targetT1size_ = (subtrahend >= targetT1size_) ? 0 : targetT1size_ - subtrahend;
                return nullptr;
            }

            case NO_LIST:
                return nullptr;
        }

        return nullptr;
    }

    void insert(const keyT& key, T page) override {
        auto hit = hash_.find(key);

        if (hit != hash_.end() && (hit->second.listName == B1 || hit->second.listName == B2)) {
            moveGhostPageToT2(hit, std::move(page));
            return;
        }

        const size_t recentTotal = T1_.size() + B1_.size();

        if (recentTotal == capacity_) {
            if (T1_.size() < capacity_) {
                eraseLRU(B1_);
                replacePage(false);
            } else {
                eraseLRU(T1_);
            }
        } else if (recentTotal < capacity_) {
            const size_t total = T1_.size() + T2_.size() + B1_.size() + B2_.size();

            if (total >= capacity_) {
                if (total >= 2 * capacity_) {
                    eraseLRU(B2_);
                }
                replacePage(false);
            }
        }

        T1_.push_front(entry_t{key, std::optional<T>{std::move(page)}});
        hash_.emplace(key, pageLoc_t{T1_.begin(), T1});
    }
};

} // namespace cache
