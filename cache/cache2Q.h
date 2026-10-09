#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "cache.h"

namespace cache {

template <typename T, typename KeyT = int>
class Cache2Q : public Cache<T, KeyT> {
private:
    using Base = Cache<T, KeyT>;
    using typename Base::PageResult;

    enum class ResidentQueue { A1_IN, AM };

    struct PageRecord {
        KeyT key;
        T page;

        PageRecord(const KeyT& pageKey, const T& pageValue) : key(pageKey), page(pageValue) {}
    };

    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;
    using GhostHistory = std::list<KeyT>;
    using GhostIterator = typename GhostHistory::iterator;

    struct ResidentPosition {
        PageIterator iterator;
        ResidentQueue queue;
    };

    // Ghosts have key iterators; residents have page iterators and queue names.
    using QueuePosition = std::variant<ResidentPosition, GhostIterator>;
    using PageIndex = std::unordered_map<KeyT, QueuePosition>;
    using IndexIterator = typename PageIndex::iterator;

    static_assert(std::is_nothrow_assignable_v<QueuePosition&, ResidentPosition> &&
                      std::is_nothrow_assignable_v<QueuePosition&, GhostIterator>,
                  "Queue transitions must not throw");
    static_assert(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<KeyT>,
                  "Pages and keys must have non-throwing destructors");

    static constexpr std::size_t a1inTargetSizeDivisor = 4;
    static constexpr std::size_t ghostLimitDivisor = 2;

    const std::size_t a1inTargetSize_;
    const std::size_t ghostLimit_;
    PageList a1in_;
    PageList am_;
    GhostHistory a1out_;
    PageIndex pageIndex_;

    struct EvictionPlan {
        IndexIterator residentToEvict;
        IndexIterator ghostToForget;
    };

    PageList& residentQueue(ResidentQueue queue) noexcept {
        return queue == ResidentQueue::A1_IN ? a1in_ : am_;
    }

    EvictionPlan prepareEviction(bool promotingGhost, GhostHistory& stagedHistory) {
        EvictionPlan plan{pageIndex_.end(), pageIndex_.end()};
        if (getResidentCount() < this->getSize()) {
            return plan;
        }

        const bool evictFromA1in = a1in_.size() > a1inTargetSize_ || am_.empty();
        if (!evictFromA1in) {
            plan.residentToEvict = pageIndex_.find(am_.back().key);
            return plan;
        }

        plan.residentToEvict = pageIndex_.find(a1in_.back().key);
        stagedHistory.emplace_front(a1in_.back().key);

        if (!promotingGhost && a1out_.size() >= ghostLimit_) {
            plan.ghostToForget = pageIndex_.find(a1out_.back());
        }
        return plan;
    }

    void forgetGhost(IndexIterator indexedGhost) noexcept {
        auto ghostIterator = std::get<GhostIterator>(indexedGhost->second);
        pageIndex_.erase(indexedGhost);
        a1out_.erase(ghostIterator);
    }

    void commitEviction(const EvictionPlan& plan, GhostHistory& stagedHistory) noexcept {
        if (plan.ghostToForget != pageIndex_.end()) {
            forgetGhost(plan.ghostToForget);
        }
        if (plan.residentToEvict == pageIndex_.end()) {
            return;
        }

        auto position = std::get<ResidentPosition>(plan.residentToEvict->second);
        if (position.queue == ResidentQueue::A1_IN) {
            auto ghostIterator = stagedHistory.begin();
            a1out_.splice(a1out_.begin(), stagedHistory, ghostIterator);
            plan.residentToEvict->second = ghostIterator;
        } else {
            pageIndex_.erase(plan.residentToEvict);
        }
        residentQueue(position.queue).erase(position.iterator);
    }

    const T& insertNewPage(const KeyT& key, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(key, page);
        auto [indexedPage, wasInserted] =
            pageIndex_.emplace(key, ResidentPosition{stagedPage.begin(), ResidentQueue::A1_IN});
        if (!wasInserted) {
            throw std::logic_error("insertNewPage requires an unknown key");
        }

        GhostHistory stagedHistory;
        EvictionPlan plan{pageIndex_.end(), pageIndex_.end()};
        try {
            plan = prepareEviction(false, stagedHistory);
        } catch (...) {
            pageIndex_.erase(indexedPage);
            throw;
        }

        commitEviction(plan, stagedHistory);
        a1in_.splice(a1in_.begin(), stagedPage, stagedPage.begin());
        return a1in_.front().page;
    }

    const T& promoteGhostPage(IndexIterator indexedGhost, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(indexedGhost->first, page);
        GhostHistory stagedHistory;
        auto plan = prepareEviction(true, stagedHistory);
        auto previousGhost = std::get<GhostIterator>(indexedGhost->second);

        commitEviction(plan, stagedHistory);

        auto pageIterator = stagedPage.begin();
        am_.splice(am_.begin(), stagedPage, pageIterator);
        indexedGhost->second = ResidentPosition{pageIterator, ResidentQueue::AM};

        a1out_.erase(previousGhost);
        return pageIterator->page;
    }

    void recordHit(ResidentPosition& resident) {
        if (resident.queue == ResidentQueue::AM) {
            am_.splice(am_.begin(), am_, resident.iterator);
        }
    }

public:
    // Johnson/Shasha defaults: A1_IN target 25%, ghost history limit 50%.
    // https://www.vldb.org/conf/1994/P439.PDF
    explicit Cache2Q(std::size_t capacity, CacheLevel level = CacheLevel::L1)
        : Base(capacity, level),
          a1inTargetSize_(std::max<std::size_t>(1, capacity / a1inTargetSizeDivisor)),
          ghostLimit_(std::max<std::size_t>(1, capacity / ghostLimitDivisor)) {
        if (capacity < 2) {
            throw std::invalid_argument("2Q requires at least 2 cache slots");
        }
    }

    std::size_t getResidentCount() const noexcept { return a1in_.size() + am_.size(); }
    std::size_t getIndexedCount() const noexcept { return pageIndex_.size(); }
    std::size_t getA1inTargetSize() const noexcept { return a1inTargetSize_; }
    std::size_t getGhostLimit() const noexcept { return ghostLimit_; }
    const PageList& getAm() const noexcept { return am_; }
    const PageList& getA1in() const noexcept { return a1in_; }
    const GhostHistory& getA1out() const noexcept { return a1out_; }

protected:
    PageResult getPage(const KeyT& key) override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            return std::nullopt;
        }
        auto resident = std::get_if<ResidentPosition>(&indexedRecord->second);
        if (resident == nullptr) {
            return std::nullopt;
        }

        recordHit(*resident);

        return std::cref(resident->iterator->page);
    }

    const T& insert(const KeyT& key, const T& page) override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            return insertNewPage(key, page);
        }

        if (std::holds_alternative<ResidentPosition>(indexedRecord->second)) {
            throw std::logic_error("insert requires a non-resident key");
        }
        return promoteGhostPage(indexedRecord, page);
    }
};

} // namespace cache
