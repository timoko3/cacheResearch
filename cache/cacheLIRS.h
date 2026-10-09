#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "cache.h"

namespace cache {

template <typename T, typename keyT = int>
class CacheLIRS : public Cache<T, keyT> {
private:
    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;

    enum class PageStatus { LIR, HIR };

    struct PageRecord {
        keyT key;
        T page;

        PageRecord(const keyT& pageKey, const T& pageValue) : key(pageKey), page(pageValue) {}
    };

    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;
    using KeyList = std::list<keyT>;
    using KeyIterator = typename KeyList::iterator;

    struct PagePosition {
        // LIR: resident + S, no Q. Resident HIR: Q, optionally S.
        // Non-resident HIR: S only; the page itself has been evicted.
        PageStatus status = PageStatus::HIR;
        std::optional<PageIterator> resident;
        std::optional<KeyIterator> stackIterator;
        std::optional<KeyIterator> queueIterator;
    };

    using PageIndex = std::unordered_map<keyT, PagePosition>;
    using IndexIterator = typename PageIndex::iterator;

    static_assert(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<keyT>,
                  "Pages and keys must have non-throwing destructors");

    static constexpr std::size_t hirTargetSizeDivisor = 100;
    const std::size_t hirTargetSize_;
    const std::size_t lirTargetSize_;
    std::size_t lirCount_ = 0;
    PageList residentPages_;
    KeyList stackS_;
    KeyList queueQ_;
    PageIndex pageIndex_;

    enum class AccessAction { RefreshLIR, RefreshHIR, PromoteHIR };

    // Preparation may throw; applying a prepared plan only splices/erases nodes.
    struct AccessPlan {
        AccessAction action = AccessAction::RefreshHIR;
        std::optional<IndexIterator> lirToDemote;
        std::optional<IndexIterator> hirToEvict;
        KeyList stagedStack;
        KeyList stagedQueue;
        std::vector<IndexIterator> stackToPrune;
    };

    AccessAction accessAction(const PagePosition& position) const noexcept {
        if (position.status == PageStatus::LIR) {
            return AccessAction::RefreshLIR;
        }
        // HIR in S is promoted on reuse. During warm-up, fill vacant LIR slots.
        if (position.stackIterator || (!position.resident && lirCount_ < lirTargetSize_)) {
            return AccessAction::PromoteHIR;
        }
        return AccessAction::RefreshHIR;
    }

    void prepareStackPrune(const PagePosition& accessed, AccessPlan& plan) {
        // Examine the future bottom of S: skip the accessed key (moving to
        // the top) and treat the demotion victim as HIR. Stop at the next LIR.
        auto iterator = stackS_.end();
        while (iterator != stackS_.begin()) {
            --iterator;
            if (accessed.stackIterator && iterator == *accessed.stackIterator) {
                continue;
            }

            auto indexedRecord = pageIndex_.find(*iterator);
            const bool willBeDemoted = plan.lirToDemote && indexedRecord == *plan.lirToDemote;
            if (indexedRecord->second.status == PageStatus::LIR && !willBeDemoted) {
                break;
            }
            plan.stackToPrune.push_back(indexedRecord);
        }
    }

    AccessPlan prepareAccess(const keyT& key, const PagePosition& position) {
        AccessPlan plan;
        plan.action = accessAction(position);
        if (!position.stackIterator) {
            plan.stagedStack.emplace_front(key);
        }

        if (plan.action == AccessAction::PromoteHIR && lirCount_ >= lirTargetSize_) {
            plan.lirToDemote = pageIndex_.find(stackS_.back());
            plan.stagedQueue.emplace_front((*plan.lirToDemote)->first);
        } else if (plan.action == AccessAction::RefreshHIR && !position.queueIterator) {
            plan.stagedQueue.emplace_front(key);
        }

        if (plan.action != AccessAction::RefreshHIR) {
            prepareStackPrune(position, plan);
        }
        return plan;
    }

    void moveToStackTop(PagePosition& position, KeyList& stagedStack) noexcept {
        if (position.stackIterator) {
            stackS_.splice(stackS_.begin(), stackS_, *position.stackIterator);
        } else {
            auto iterator = stagedStack.begin();
            stackS_.splice(stackS_.begin(), stagedStack, iterator);
            position.stackIterator = iterator;
        }
    }

    void moveToQueueFront(PagePosition& position, KeyList& stagedQueue) noexcept {
        if (position.queueIterator) {
            queueQ_.splice(queueQ_.begin(), queueQ_, *position.queueIterator);
        } else {
            auto iterator = stagedQueue.begin();
            queueQ_.splice(queueQ_.begin(), stagedQueue, iterator);
            position.queueIterator = iterator;
        }
    }

    void pruneStack(const AccessPlan& plan) noexcept {
        for (auto indexedRecord : plan.stackToPrune) {
            auto& position = indexedRecord->second;
            stackS_.erase(*position.stackIterator);
            position.stackIterator.reset();
            if (!position.resident) {
                pageIndex_.erase(indexedRecord);
            }
        }
    }

    void promoteToLIR(PagePosition& position, AccessPlan& plan) noexcept {
        if (position.queueIterator) {
            queueQ_.erase(*position.queueIterator);
            position.queueIterator.reset();
        }
        position.status = PageStatus::LIR;
        ++lirCount_;

        if (plan.lirToDemote) {
            auto& victim = (*plan.lirToDemote)->second;
            moveToQueueFront(victim, plan.stagedQueue);
            victim.status = PageStatus::HIR;
            --lirCount_;
        }
    }

    void evictHIR(IndexIterator indexedRecord) noexcept {
        auto& position = indexedRecord->second;
        queueQ_.erase(*position.queueIterator);
        position.queueIterator.reset();
        residentPages_.erase(*position.resident);
        position.resident.reset();
        if (!position.stackIterator) {
            pageIndex_.erase(indexedRecord);
        }
    }

    void applyAccess(PagePosition& position, AccessPlan& plan) noexcept {
        moveToStackTop(position, plan.stagedStack);
        if (plan.action == AccessAction::PromoteHIR) {
            promoteToLIR(position, plan);
        }
        if (plan.action == AccessAction::RefreshHIR) {
            moveToQueueFront(position, plan.stagedQueue);
        } else {
            pruneStack(plan);
        }
    }

    void recordHit(const keyT& key, PagePosition& position) {
        auto plan = prepareAccess(key, position);
        applyAccess(position, plan);
    }

    const T& insertPage(const keyT& key, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(key, page);
        auto [indexedRecord, wasInserted] = pageIndex_.try_emplace(key);
        auto& position = indexedRecord->second;
        if (position.resident) {
            throw std::logic_error("insert requires a non-resident key");
        }

        AccessPlan plan;
        try {
            // try_emplace may rehash: obtain all other index iterators afterwards.
            plan = prepareAccess(key, position);
            if (getResidentCount() >= this->getSize()) {
                plan.hirToEvict = pageIndex_.find(queueQ_.back());
            }
        } catch (...) {
            if (wasInserted) {
                pageIndex_.erase(indexedRecord);
            }
            throw;
        }

        if (plan.hirToEvict) {
            evictHIR(*plan.hirToEvict);
        }
        auto resident = stagedPage.begin();
        residentPages_.splice(residentPages_.begin(), stagedPage, resident);
        position.resident = resident;
        applyAccess(position, plan);
        return resident->page;
    }

public:
    // Jiang/Zhang: resident HIR = 1%, LIR gets the remaining capacity.
    // https://xiaodongzhang1911.github.io/Zhang-papers/TR-05-11.pdf
    explicit CacheLIRS(std::size_t capacity, cacheLevel_t level = cacheLevel_t::L1)
        : Base(capacity, level),
          hirTargetSize_(std::max<std::size_t>(1, capacity / hirTargetSizeDivisor)),
          lirTargetSize_(capacity >= 2 ? capacity - hirTargetSize_ : 0) {
        if (capacity < 2) {
            throw std::invalid_argument("LIRS requires at least 2 cache slots");
        }
    }

    std::size_t getResidentCount() const noexcept { return residentPages_.size(); }
    std::size_t getIndexedCount() const noexcept { return pageIndex_.size(); }
    std::size_t getLIRCount() const noexcept { return lirCount_; }
    std::size_t getLIRTargetSize() const noexcept { return lirTargetSize_; }
    std::size_t getHIRTargetSize() const noexcept { return hirTargetSize_; }
    const KeyList& getStackS() const noexcept { return stackS_; }
    const KeyList& getQueueQ() const noexcept { return queueQ_; }
    const PageList& getResidentPages() const noexcept { return residentPages_; }

protected:
    pageResult_t getPage(const keyT& key) override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end() || !indexedRecord->second.resident) {
            return std::nullopt;
        }

        auto& position = indexedRecord->second;
        recordHit(key, position);

        return std::cref((*position.resident)->page);
    }

    const T& insert(const keyT& key, const T& page) override { return insertPage(key, page); }
};

} // namespace cache
