#pragma once

#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <vector>

namespace cache {

enum cacheLevel { L1, L2, L3, L4, L5};

enum cacheEvictionType{
    C_LRU,
    C_LFU,
    C_ARC,
    C_LIRS,
    C_2Q,
    C_REF,
    C_UNKNOWN
};

struct cacheDescription{
    size_t size;
    cacheLevel level;
    cacheEvictionType strategy;
};

struct CacheStats {
  size_t amountRequests = 0;
  size_t amountHits = 0;
  size_t amountMisses = 0;
};

template <typename T, typename keyT = int> class Cache {
  cacheLevel level_;
  size_t size_;

  CacheStats stats_;

public:
  explicit Cache(size_t size, cacheLevel level = L1)
      : level_(level), size_(size) {}

  virtual ~Cache() = default;

  cacheLevel getLevel() const { return level_; }
  size_t getSize() const { return size_; }
  const CacheStats &getStats() const { return stats_; }

  template <typename F> T lookupUpdate(keyT key, F slow_get_page) {
    ++stats_.amountRequests;

    if (const T* page = findAndTouch(key)) {
      ++stats_.amountHits;
      return *page;
    }

    ++stats_.amountMisses;
    T page = slow_get_page(key);

    if (size_ != 0) {
      insert(key, page);
    }

    return page;
  }

protected:
  // Returns a resident page, or nullptr on a miss (including ghost entries).
  virtual const T* findAndTouch(const keyT &key) = 0;

  virtual void insert(const keyT &key, T page) = 0;
};

template <typename T, typename keyT = int>
class CacheLRU : public Cache<T, keyT> {
private:
  using Base = Cache<T, keyT>;
  using Entry = std::pair<keyT, T>;
  using List = std::list<Entry>;
  using ListIt = typename List::iterator;

  List cache_;
  std::unordered_map<keyT, ListIt> hash_;

public:
  explicit CacheLRU(size_t size, cacheLevel level = L1) : Base(size, level) {}

  const List &getCache() const { return cache_; }

  const std::unordered_map<keyT, ListIt> &getHash() const { return hash_; }

  bool isFull() const { return cache_.size() >= this->getSize(); }

protected:
  const T* findAndTouch(const keyT &key) override {
    auto hit = hash_.find(key);

    if (hit == hash_.end()) {
      return nullptr;
    }

    cache_.splice(cache_.begin(), cache_, hit->second);
    return std::addressof(hit->second->second);
  }

  void insert(const keyT &key, T page) override {
    if (isFull()) {
      hash_.erase(cache_.back().first);
      cache_.pop_back();
    }

    cache_.emplace_front(key, std::move(page));
    hash_.emplace(key, cache_.begin());
  }
};

template <typename T, typename keyT = int>
class Cache2Q : public Cache<T, keyT> {
private:
  enum queueType { A1_IN, A1_OUT, AM };

  using Base = Cache<T, keyT>;
  using Entry = std::pair<keyT, T>;
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
      : Base(size, level),
        KIn_(std::max<size_t>(1, size / 4)),
        KOut_(std::max<size_t>(1, size / 2)),
        AmSize_(0) {
    if (size < 2) {
      throw std::invalid_argument("2Q requires at least 2 cache slots");
    }

    AmSize_ = size - KIn_;
  }

  const std::unordered_map<keyT, elemLoc> &getHash() const { return hash_; }

  const List &getAm() const { return Am_; }

  const List &getA1in() const { return A1in_; }

  const List &getA1out() const { return A1out_; }

  bool isFullAIn() const { return A1in_.size() >= KIn_; }

  bool isFullAOut() const { return A1out_.size() > KOut_; }

  bool isFullAm() const { return Am_.size() >= AmSize_; }

protected:
  const T* findAndTouch(const keyT &key) override {
    auto hit = hash_.find(key);

    if (hit == hash_.end()) {
      return nullptr;
    }

    auto reqType = hit->second.second;
    auto curIt = hit->second.first;

    switch (reqType) {
    case A1_IN:
      return std::addressof(curIt->second);
      break;
    case A1_OUT:
      isGhostHit_ = true;

      return nullptr;
      break;
    case AM:
      Am_.splice(Am_.begin(), Am_, curIt);

      return std::addressof(curIt->second);
      break;
    }

    return nullptr;
  }

  void insert(const keyT &key, T page) override {
    if (!isGhostHit_) {
      if (isFullAIn()) {
        auto victim = std::prev(A1in_.end());

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
      hash_.at(key).first->second = std::move(page);

      Am_.splice(Am_.begin(), A1out_, hash_.find(key)->second.first);
      hash_.at(key).second = AM;

      isGhostHit_ = false;
    }
  }
};

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

  void moveToStackTop(const keyT &key, Entry &entry) {
    if (entry.stackIt.has_value()) {
      stackS_.splice(stackS_.begin(), stackS_, *entry.stackIt);
    } else {
      stackS_.push_front(key);
      entry.stackIt = stackS_.begin();
    }
  }

  void moveToQueueFront(const keyT &key, Entry &entry) {
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
      auto &entry = hit->second;

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

  void promoteToLIR(Entry &entry) {
    if (entry.queueIt.has_value()) {
      queueQ_.erase(*entry.queueIt);
      entry.queueIt.reset();
    }

    entry.status = Status::LIR;
    ++lirCount_;

    if (lirCount_ > sizeLIR_) {
      auto &victim = hash_.at(stackS_.back());
      moveToQueueFront(stackS_.back(), victim);
      victim.status = Status::HIR;
      --lirCount_;
    }

    pruneStack();
  }

  void evictHIR() {
    auto hit = hash_.find(queueQ_.back());
    auto &entry = hit->second;

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
      : Base(size, level), sizeLIR_(0),
        sizeHIR_(std::max<size_t>(1, size / 100)) {
    if (size < 2) {
      throw std::invalid_argument("LIRS requires at least 2 cache slots");
    }

    sizeLIR_ = size - sizeHIR_;
  }

protected:
  const T* findAndTouch(const keyT &key) override {
    auto hit = hash_.find(key);

    if (hit == hash_.end() || !hit->second.value.has_value()) {
      return nullptr;
    }

    auto &entry = hit->second;
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

  void insert(const keyT &key, T page) override {
    if (lirCount_ + queueQ_.size() >= this->getSize()) {
      evictHIR();
    }

    auto result = hash_.try_emplace(
        key, Entry{Status::HIR, std::nullopt, std::nullopt, std::nullopt});
    auto &entry = result.first->second;
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

template <typename T, typename keyT = int>
class CacheARC : public Cache<T, keyT> {
private:
    enum listName_t {
        NO_LIST,
        T1,
        T2,
        B1,
        B2
    };

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

    void eraseLRU(list_t &list) {
        if (list.empty()) {
            return;
        }

        auto victim = std::prev(list.end());

        hash_.erase(victim->key);
        list.erase(victim);
    }

    void moveLRUToGhost(list_t &srcList, list_t &ghostList, listName_t ghostListName) {
        if (srcList.empty()) {
            return;
        }

        auto victim = std::prev(srcList.end());

        victim->value.reset();

        ghostList.splice(ghostList.begin(), srcList, victim);

        auto &location = hash_.at(victim->key);

        location.listIt = victim;
        location.listName = ghostListName;
    }

    void replacePage(bool requestedFromB2) {
        const bool evictFromT1 = !T1_.empty() &&
        (T1_.size() > targetT1size_ || (requestedFromB2 && T1_.size() == targetT1size_));

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
        : Base(capacity, level),
          capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument(
                "ARC cache capacity must be greater than zero"
            );
        }
    }

protected:
    const T* findAndTouch(const keyT &key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return nullptr;
        }

        auto &location = hit->second;
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

    void insert(const keyT &key, T page) override {
        auto hit = hash_.find(key);

        if (hit != hash_.end() &&
           (hit->second.listName == B1 || hit->second.listName == B2)) {
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
            const size_t total = T1_.size() + T2_.size() +
                                 B1_.size() + B2_.size();

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

template <typename T, typename keyT = int>
class CacheLFU : public Cache<T, keyT>
{
private:

    struct Entry_t 
    {
        keyT key;
        T page;
        std::size_t frequency = 0;
    };

    using Base_t   = Cache<T, keyT>;
    using List_t   = std::list<Entry_t>;
    using ListIt_t = typename List_t::iterator;

    std::unordered_map<keyT, ListIt_t> hash_;
    List_t cache_;

    static bool compareEntriesByFreq(const Entry_t& src1, const Entry_t& src2)
    {
        return src1.frequency < src2.frequency;
    }

    bool isFull() const 
    { 
        return cache_.size() >= this->getSize(); 
    }

public:
    explicit CacheLFU(size_t size, cacheLevel level = L1) : Base_t(size, level) {};

    ~CacheLFU() = default;

protected:
    const T* findAndTouch(const keyT &key) override
    {
        auto hit = hash_.find(key);

        if (hit == hash_.end())
        {
            return nullptr;
        }

        auto entryIt = hit->second;
        entryIt->frequency++;
        cache_.splice(cache_.end(), cache_, entryIt);
        return std::addressof(entryIt->page);
    }

    void insert(const keyT &key, T page) override
    {
        if (isFull()) 
        {
            auto victim = std::min_element(cache_.begin(), cache_.end(), compareEntriesByFreq);
            hash_.erase(victim->key);
            cache_.erase(victim);
        }
        
        Entry_t new_entry = {key, page, 1};
        cache_.push_back(new_entry);
        hash_.emplace(key, std::prev(cache_.end()));
    }

};

template <typename T, typename keyT = int>
class CacheREF : public Cache<keyT, T>
{
private:
    struct Entry_t
    {
        keyT key;
        T page;
    };

    using Base_t      = Cache<T, keyT>;
    using CacheList_t = std::list<Entry_t>;
    using ReqList_t   = std::list<keyT>;
    using CacheIt_t   = typename CacheList_t::iterator;
    using ReqIt_t     = typename ReqList_t::iterator;
    using HashTable_t = std::unordered_map<keyT, CacheIt_t>;
    using Dist_t      = typename ReqList_t::difference_type;
    
    HashTable_t hash_;
    CacheList_t cache_;
    ReqList_t requests_;
    ReqIt_t pos_;

    bool isFull () { return cache_.size() >= this->getSize(); };

    CacheIt_t findVictim ()
    {
        CacheIt_t victim_it = cache_.begin();
        Dist_t max_dist = 0;

        for (auto cache_it = cache_.begin(); cache_it != cache_.end(); ++cache_it)
        {
            Dist_t dist_to_next = requests_.size();

            for (auto req_it = pos_; req_it != requests_.end(); ++req_it)
            {
                if (*req_it == cache_it->key)
                {
                    dist_to_next = std::distance(pos_, req_it);
                    break;
                } 
            } 
            
            if (dist_to_next >= max_dist)
            {
                max_dist = dist_to_next;
                victim_it = cache_it;
            }
        }

        return victim_it;
    };

public:
    explicit CacheREF(size_t size, ReqList_t reqs, cacheLevel level = L1) 
                     : Base_t(size, level), requests_(reqs), pos_(requests_.begin())
    {
        if (requests_.empty())
        {
            throw std::invalid_argument("Array with requests are empty");
        }
    };

    ~CacheREF() = default;

protected:
    bool findAndTouch(const keyT &key) override
    {
        ++pos_;

        auto hit = hash_.find(key);

        if (hit == hash_.end())
        {
            return false;
        }

        return true;
    }

    void insert(const keyT &key, T page) override
    {
        if (isFull())
        {
            auto victim_it = findVictim();
            hash_.erase(victim_it->key);
            cache_.erase(victim_it);
        }

        cache_.push_front({key, page});
        hash_.emplace(key, cache_.begin());
    }
};


} // namespace cache
