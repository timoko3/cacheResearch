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
class Cache2Q : public Cache<T, keyT> {
private:
    enum queueType { A1_IN, A1_OUT, AM };

    using Base = Cache<T, keyT>;
    using Entry = std::pair<keyT, std::optional<T>>;
    using List = std::list<Entry>;
    using ListIt = typename List::iterator;
    using elemLoc = std::pair<ListIt, queueType>;

    size_t KIn_, KOut_, AmSize_;

    List Am_;
    List A1in_;

    List A1out_;

    std::unordered_map<keyT, elemLoc> hash_;

    bool isGhostHit_ = false;

public:
    // Johnson/Shasha: A1in = 25%, A1out = 50% of cache capacity.
    // https://www.openu.ac.il/home/wiseman/2os/lru/2q.pdf
    explicit Cache2Q(size_t size, cacheLevel level = L1)
        : Base(size, level), KIn_(std::max<size_t>(1, size / 4)),
          KOut_(std::max<size_t>(1, size / 2)), AmSize_(0) {
        if (size < 2) {
            throw std::invalid_argument("2Q requires at least 2 cache slots");
        }

        AmSize_ = size - KIn_;
    }

    const std::unordered_map<keyT, elemLoc>& getHash() const { return hash_; }

    const List& getAm() const { return Am_; }

    const List& getA1in() const { return A1in_; }

    const List& getA1out() const { return A1out_; }

    bool isFullAIn() const { return A1in_.size() >= KIn_; }

    bool isFullAOut() const { return A1out_.size() > KOut_; }

    bool isFullAm() const { return Am_.size() >= AmSize_; }

protected:
    const T* findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        auto reqType = hit->second.second;
        auto curIt = hit->second.first;

        switch (reqType) {
            case A1_IN:
                return std::addressof(*curIt->second);
                break;
            case A1_OUT:
                isGhostHit_ = true;

                return nullptr;
                break;
            case AM:
                Am_.splice(Am_.begin(), Am_, curIt);

                return std::addressof(*curIt->second);
                break;
        }

        return nullptr;
    }

    void insert(const keyT& key, T page) override {
        if (!isGhostHit_) {
            if (isFullAIn()) {
                auto victim = std::prev(A1in_.end());

                victim->second.reset();
                A1out_.splice(A1out_.begin(), A1in_, victim);
                hash_.at(victim->first).second = A1_OUT;
            }

            if (isFullAOut()) {
                hash_.erase(A1out_.back().first);
                A1out_.pop_back();
            }

            A1in_.emplace_front(key, std::move(page));
            hash_.emplace(key, elemLoc{A1in_.begin(), A1_IN});
        } else {
            if (isFullAm()) {
                hash_.erase(Am_.back().first);
                Am_.pop_back();
            }
            hash_.at(key).first->second.emplace(std::move(page));

            Am_.splice(Am_.begin(), A1out_, hash_.find(key)->second.first);
            hash_.at(key).second = AM;

            isGhostHit_ = false;
        }
    }
};

} // namespace cache
