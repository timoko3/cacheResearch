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

template <typename T, typename keyT = int>
class Cache2Q : public Cache<T, keyT> {
private:
    using Base = Cache<T, keyT>;
    using typename Base::pageResult_t;

    enum class ResidentQueue { A1in, Am };

    struct PageRecord {
        keyT key;
        T page;

        PageRecord(const keyT& pageKey, const T& pageValue) : key(pageKey), page(pageValue) {}
    };

    using PageList = std::list<PageRecord>;
    using PageIterator = typename PageList::iterator;
    using GhostHistory = std::list<keyT>;
    using GhostIterator = typename GhostHistory::iterator;

    struct ResidentPosition {
        PageIterator iterator;
        ResidentQueue queue;
    };

    // Ghosts have key iterators; residents have page iterators and queue names.
    using QueuePosition = std::variant<ResidentPosition, GhostIterator>;
    using PageIndex = std::unordered_map<keyT, QueuePosition>;
    using IndexIterator = typename PageIndex::iterator;

    static_assert(std::is_nothrow_assignable_v<QueuePosition&, ResidentPosition> &&
                  std::is_nothrow_assignable_v<QueuePosition&, GhostIterator>,
                  "Queue transitions must not throw");
    static_assert(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<keyT>,
                  "Pages and keys must have non-throwing destructors");

    static constexpr std::size_t a1inTargetSizeDivisor = 4;
    static constexpr std::size_t ghostLimitDivisor = 2;

    const std::size_t a1inTargetSize_;
    const std::size_t ghostLimit_;
    PageList A1in_;
    PageList Am_;
    GhostHistory A1out_;
    PageIndex pageIndex_;

    struct EvictionPlan {
        IndexIterator residentToEvict;
        IndexIterator ghostToForget;
    };

    PageList& residentQueue(ResidentQueue queue) noexcept {
        return queue == ResidentQueue::A1in ? A1in_ : Am_;
    }

    EvictionPlan prepareEviction(bool promotingGhost, GhostHistory& stagedHistory) {
        EvictionPlan plan{pageIndex_.end(), pageIndex_.end()};
        if (getResidentCount() < this->getSize()) {
            return plan;
        }

        const bool evictFromA1in = A1in_.size() > a1inTargetSize_ || Am_.empty();
        if (!evictFromA1in) {
            plan.residentToEvict = pageIndex_.find(Am_.back().key);
            return plan;
        }

        plan.residentToEvict = pageIndex_.find(A1in_.back().key);
        stagedHistory.emplace_front(A1in_.back().key);
        
        if (!promotingGhost && A1out_.size() >= ghostLimit_) {
            plan.ghostToForget = pageIndex_.find(A1out_.back());
        }
        return plan;
    }

    void forgetGhost(IndexIterator indexedGhost) noexcept {
        auto ghostIterator = std::get<GhostIterator>(indexedGhost->second);
        pageIndex_.erase(indexedGhost);
        A1out_.erase(ghostIterator);
    }

    void commitEviction(const EvictionPlan& plan, GhostHistory& stagedHistory) noexcept {
        if (plan.ghostToForget != pageIndex_.end()) {
            forgetGhost(plan.ghostToForget);
        }
        if (plan.residentToEvict == pageIndex_.end()) {
            return;
        }

        auto position = std::get<ResidentPosition>(plan.residentToEvict->second);
        if (position.queue == ResidentQueue::A1in) {
            auto ghostIterator = stagedHistory.begin();
            A1out_.splice(A1out_.begin(), stagedHistory, ghostIterator);
            plan.residentToEvict->second = ghostIterator;
        } else {
            pageIndex_.erase(plan.residentToEvict);
        }
        residentQueue(position.queue).erase(position.iterator);
    }

    void insertNewPage(const keyT& key, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(key, page);
        auto [indexedPage, wasInserted] = pageIndex_.emplace(
            key, ResidentPosition{stagedPage.begin(), ResidentQueue::A1in});
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
        A1in_.splice(A1in_.begin(), stagedPage, stagedPage.begin());
    }

    void promoteGhostPage(IndexIterator indexedGhost, const T& page) {
        PageList stagedPage;
        stagedPage.emplace_front(indexedGhost->first, page);
        GhostHistory stagedHistory;
        auto plan = prepareEviction(true, stagedHistory);
        auto previousGhost = std::get<GhostIterator>(indexedGhost->second);

        commitEviction(plan, stagedHistory);

        auto pageIterator = stagedPage.begin();
        Am_.splice(Am_.begin(), stagedPage, pageIterator);
        indexedGhost->second = ResidentPosition{pageIterator, ResidentQueue::Am};

        A1out_.erase(previousGhost);
    }

    void recordHit(ResidentPosition& resident){
        if (resident.queue == ResidentQueue::Am) {
            Am_.splice(Am_.begin(), Am_, resident.iterator);
        }
    }

public:
    // Johnson/Shasha defaults: A1in target 25%, ghost history limit 50%.
    // https://www.vldb.org/conf/1994/P439.PDF
    explicit Cache2Q(std::size_t capacity, cacheLevel_t level = cacheLevel_t::L1)
        : Base(capacity, level),
          a1inTargetSize_(std::max<std::size_t>(1, capacity / a1inTargetSizeDivisor)),
          ghostLimit_(std::max<std::size_t>(1, capacity / ghostLimitDivisor)) {
        if (capacity < 2) {
            throw std::invalid_argument("2Q requires at least 2 cache slots");
        }
    }

    std::size_t getResidentCount() const noexcept { return A1in_.size() + Am_.size(); }
    std::size_t getIndexedCount() const noexcept { return pageIndex_.size(); }
    std::size_t getA1inTargetSize() const noexcept { return a1inTargetSize_; }
    std::size_t getGhostLimit() const noexcept { return ghostLimit_; }
    const PageList& getAm() const noexcept { return Am_; }
    const PageList& getA1in() const noexcept { return A1in_; }
    const GhostHistory& getA1out() const noexcept { return A1out_; }

protected:
    pageResult_t getPage(const keyT& key) override {
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

    void insert(const keyT& key, const T& page) override {
        auto indexedRecord = pageIndex_.find(key);
        if (indexedRecord == pageIndex_.end()) {
            insertNewPage(key, page);
            return;
        }

        if (std::holds_alternative<ResidentPosition>(indexedRecord->second)) {
            throw std::logic_error("insert requires a non-resident key");
        }
        promoteGhostPage(indexedRecord, page);
    }
};

} // namespace cache
