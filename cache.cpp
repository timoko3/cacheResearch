#include "cache.h"

namespace cache
{

template <typename T, typename keyT>
template <typename F>
bool 
cache_t<T, keyT>::lookupUpdate(keyT key, F slow_get_page)
{
    stats_.amountRequests++;

    auto hit = hash_.find(key);
    if( hit != hash_.end() )
    {
        auto elementIt = hit->second;
        cache_.splice(cache_.begin(), cache_, elementIt);

        stats_.amountHits++;

        return true;
    }

    T page = slow_get_page(key);

    if( full() ){
        hash_.erase(cache_.back().first);
        cache_.pop_back();
    }

    cache_.emplace_front(key, page);
    hash_.emplace(key, cache_.begin());
    return false;
}

template bool cache_t<int, int>::lookupUpdate<int (*)(int)>(
    int, int (*)(int));

}