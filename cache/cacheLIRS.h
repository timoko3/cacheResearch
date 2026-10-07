#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheLIRS : public Cache<T, keyT> {
    using Base = Cache<T, keyT>;
    using KeyList = std::list<keyT>;
    using KeyIt = typename KeyList::iterator;

    enum class Status { LIR, HIR };

    struct Entry {
        Status status;
        std::optional<T> value;
        std::optional<KeyIt> stackIt;
        std::optional<KeyIt> queueIt;
    };

    KeyList stackS_;
    KeyList queueQ_;
    std::unordered_map<keyT, Entry> hash_;

    size_t sizeLIR_, sizeHIR_;
    size_t lirCount_ = 0;

    void moveToStackTop(const keyT& key, Entry& entry) {
        if (entry.stackIt.has_value()) {
            stackS_.splice(stackS_.begin(), stackS_, *entry.stackIt);
        } else {
            stackS_.push_front(key);
            entry.stackIt = stackS_.begin();
        }
    }

    void moveToQueueFront(const keyT& key, Entry& entry) {
        if (entry.queueIt.has_value()) {
            queueQ_.splice(queueQ_.begin(), queueQ_, *entry.queueIt);
        } else {
            queueQ_.push_front(key);
            entry.queueIt = queueQ_.begin();
        }
    }

    void pruneStack() {
        while (!stackS_.empty()) {
            auto hit = hash_.find(stackS_.back());
            auto& entry = hit->second;

            if (entry.status == Status::LIR) {
                break;
            }

            entry.stackIt.reset();
            stackS_.pop_back();

            if (!entry.value.has_value()) {
                hash_.erase(hit);
            }
        }
    }

    void promoteToLIR(Entry& entry) {
        if (entry.queueIt.has_value()) {
            queueQ_.erase(*entry.queueIt);
            entry.queueIt.reset();
        }

        entry.status = Status::LIR;
        ++lirCount_;

        if (lirCount_ > sizeLIR_) {
            auto& victim = hash_.at(stackS_.back());
            moveToQueueFront(stackS_.back(), victim);
            victim.status = Status::HIR;
            --lirCount_;
        }

        pruneStack();
    }

    void evictHIR() {
        auto hit = hash_.find(queueQ_.back());
        auto& entry = hit->second;

        queueQ_.pop_back();
        entry.queueIt.reset();
        entry.value.reset();

        if (!entry.stackIt.has_value()) {
            hash_.erase(hit);
        }
    }

public:
    // Jiang/Zhang: resident HIR = 1%, LIR gets the remaining capacity.
    // https://xiaodongzhang1911.github.io/Zhang-papers/TR-05-11.pdf
    explicit CacheLIRS(size_t size, cacheLevel level = L1)
        : Base(size, level), sizeLIR_(0), sizeHIR_(std::max<size_t>(1, size / 100)) {
        if (size < 2) {
            throw std::invalid_argument("LIRS requires at least 2 cache slots");
        }

        sizeLIR_ = size - sizeHIR_;
    }

protected:
    const T* findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end() || !hit->second.value.has_value()) {
            return nullptr;
        }

        auto& entry = hit->second;
        const bool wasInStack = entry.stackIt.has_value();
        moveToStackTop(key, entry);

        if (entry.status == Status::LIR) {
            pruneStack();
        } else if (wasInStack) {
            promoteToLIR(entry);
        } else {
            moveToQueueFront(key, entry);
        }

        return std::addressof(*entry.value);
    }

    void insert(const keyT& key, T page) override {
        if (lirCount_ + queueQ_.size() >= this->getSize()) {
            evictHIR();
        }

        auto result =
            hash_.try_emplace(key, Entry{Status::HIR, std::nullopt, std::nullopt, std::nullopt});
        auto& entry = result.first->second;
        const bool wasInStack = entry.stackIt.has_value();

        entry.value.emplace(std::move(page));
        moveToStackTop(key, entry);

        if (lirCount_ < sizeLIR_ || wasInStack) {
            promoteToLIR(entry);
        } else {
            entry.status = Status::HIR;
            moveToQueueFront(key, entry);
        }
    }
};

} // namespace cache
