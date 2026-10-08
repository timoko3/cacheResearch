#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class Cache2Q : public Cache<T, keyT> {
private:
    enum class queueType { A1_IN, A1_OUT, AM };

    using Base = Cache<T, keyT>;
    using typename Base::pageResult;
    struct PageRecord {
        keyT key;
        std::optional<T> page;

        PageRecord(const keyT& pageKey, const T& pageValue) : key(pageKey), page(pageValue) {}
    };

    using List = std::list<PageRecord>;
    using ListIt = typename List::iterator;

    struct QueuePosition {
        ListIt iterator;
        queueType queue;
    };

    std::size_t KIn_, KOut_, AmSize_;

    List Am_;
    List A1in_;

    List A1out_;

    std::unordered_map<keyT, QueuePosition> hash_;

public:
    // Johnson/Shasha: A1in = 25%, A1out = 50% of cache capacity.
    // https://www.openu.ac.il/home/wiseman/2os/lru/2q.pdf
    explicit Cache2Q(std::size_t size, cacheLevel_t level = cacheLevel_t::L1)
        : Base(size, level), KIn_(std::max<std::size_t>(1, size / 4)),
          KOut_(std::max<std::size_t>(1, size / 2)), AmSize_(0) {
        if (size < 2) {
            throw std::invalid_argument("2Q requires at least 2 cache slots");
        }

        AmSize_ = size - KIn_;
    }

    const std::unordered_map<keyT, QueuePosition>& getHash() const { return hash_; }

    const List& getAm() const { return Am_; }
    const List& getA1in() const { return A1in_; }
    const List& getA1out() const { return A1out_; }

    bool isFullAIn() const { return A1in_.size() >= KIn_; }
    bool isFullAOut() const { return A1out_.size() > KOut_; }
    bool isFullAm() const { return Am_.size() >= AmSize_; }

protected:
    pageResult getPage(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return std::nullopt;
        }

        const auto& location = hit->second;
        auto curPageIt = location.iterator;

        switch (location.queue) {
            case queueType::A1_IN:
                return std::cref(*curPageIt->page);
            case queueType::A1_OUT:
                return std::nullopt;
            case queueType::AM:
                Am_.splice(Am_.begin(), Am_, curPageIt);

                return std::cref(*entry->page);
        }

        return std::nullopt;
    }

    void insert(const keyT& key, const T& page) override {
        auto hit = hash_.find(key);
        if (hit == hash_.end()) {
            // Prepare allocations and index lookups before changing resident queues.
            List pending;
            pending.emplace_front(key, page);
            auto [insertedEntry, wasInserted] =
                hash_.emplace(key, QueuePosition{pending.begin(), queueType::A1_IN});
            if (!wasInserted) {
                return;
            }

            auto victim = A1in_.end();
            auto victimLocation = hash_.end();
            auto oldestGhost = hash_.end();
            try {
                if (isFullAIn()) {
                    victim = std::prev(A1in_.end());
                    victimLocation = hash_.find(victim->key);
                    if (A1out_.size() >= KOut_) {
                        oldestGhost = hash_.find(A1out_.back().key);
                    }
                }
            } catch (...) {
                hash_.erase(insertedEntry);
                throw;
            }

            if (victim != A1in_.end()) {
                victim->page.reset();
                A1out_.splice(A1out_.begin(), A1in_, victim);
                victimLocation->second.queue = queueType::A1_OUT;
            }

            if (oldestGhost != hash_.end()) {
                hash_.erase(oldestGhost);
                A1out_.pop_back();
            }

            A1in_.splice(A1in_.begin(), pending, pending.begin());
        } else {
            auto& location = hit->second;
            if (location.queue != queueType::A1_OUT) {
                return;
            }

            auto ghost = location.iterator;
            auto hotVictim = hash_.end();
            if (isFullAm()) {
                hotVictim = hash_.find(Am_.back().key);
            }
            ghost->page.emplace(page);
            if (hotVictim != hash_.end()) {
                hash_.erase(hotVictim);
                Am_.pop_back();
            }
            Am_.splice(Am_.begin(), A1out_, ghost);
            location.queue = queueType::AM;
        }
    }
};

} // namespace cache
