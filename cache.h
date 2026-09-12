#pragma once

#include <stddef.h>
#include <list>
#include <unordered_map>
#include <utility>

namespace cache{

enum cacheLevel
{
    L1,
    L2
};

struct CacheStats {
    size_t amountRequests = 0;
    size_t amountHits     = 0;
};

template <typename T, typename keyT = int> 
class Cache
{
    cacheLevel level_;
    size_t     size_;
    
    CacheStats stats_;

public:
    explicit Cache(size_t size, cacheLevel level = L1)
        : level_(level), size_(size) {}

    virtual ~Cache() = default;

    cacheLevel getLevel() const {
        return level_;
    }
    size_t getSize() const {
        return size_;
    }
    const CacheStats& getStats() const {
        return stats_;
    }

    template <typename F>
    bool lookupUpdate(keyT key, F slow_get_page) {
        ++stats_.amountRequests;

        if ( findAndTouch(key) ) {
            ++stats_.amountHits;
            return true;
        }

        T page = slow_get_page(key);

        if (size_ != 0) {
            insert(key, std::move(page));
        }

        return false;
    }

protected:
    virtual bool findAndTouch(const keyT& key)   = 0;

    virtual void insert(const keyT& key, T page) = 0;
};

template <typename T, typename keyT = int> 
class CacheLRU : public Cache<T, keyT>
{   
private:
    using Base  = Cache<T, keyT>;
    using Entry = std::pair<keyT, T>;
    using List  = std::list<Entry>;
    using ListIt = typename List::iterator;

    List cache_;
    std::unordered_map<keyT, ListIt> hash_;
public:
    explicit CacheLRU(size_t size, cacheLevel level = L1)
        : Base(size, level) {}

    const List& getCache() const {
        return cache_;
    }

    const std::unordered_map<keyT, ListIt>& getHash() const {
        return hash_;
    }

    bool isFull() const {
        return cache_.size() >= this->getSize();
    }
protected:
    bool findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        if (hit == hash_.end()) {
            return false;
        }

        cache_.splice(cache_.begin(), cache_, hit->second);
        return true;
    }

    void insert(const keyT& key, T page) override {
        if (isFull()) {
            hash_.erase(cache_.back().first);
            cache_.pop_back();
        }

        cache_.emplace_front(key, std::move(page));
        hash_.emplace(key, cache_.begin());
    }
};

template <typename T, typename keyT = int>
class Cache2Q : public Cache<T, keyT>
{
private:
    enum queueType {
        A1_IN,
        A1_OUT,
        AM    
    };

    using Base    = Cache<T, keyT>;
    using Entry   = std::pair<keyT, T>;
    using List    = std::list<Entry>;
    using ListIt  = typename List::iterator;
    using elemLoc = std::pair<ListIt, queueType>;

    List Am_;
    List A1in_;

    std::list<keyT> A1out_;

    std::unordered_map<keyT, elemLoc> hash_;

    queueType curRequestType_;

    
public:
    explicit Cache2Q(size_t size, cacheLevel level = L1)
        : Base(size, level) {}
    
    const std::unordered_map<keyT, ListIt>& getHash() const {
        return hash_;
    }

    const List& getAm() const {
        return Am_;
    }

    const List& getA1in() const {
        return A1in_;
    }

    const std::list<keyT>& getA1out() const {
        return A1out_;
    }

    bool is
protected: 
    bool findAndTouch(const keyT& key) override {
        auto hit = hash_.find(key);

        curRequestType_ = hit->second->second; 

        if (hit == hash_.end()){
            return false;
        }

        switch(curRequestType_){
            case A1_IN:
                return true; 
                break;
            case A1_OUT:
                return false;
                break;
            case AM:
                cache_.splice(Am_.begin(), Am_, hit->second->first);
                return true;
                break;
        }
    }

    void insert(const keyT& key, T page) override {

    }
};

}