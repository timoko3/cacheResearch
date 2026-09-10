#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <array>

#include "generalFunctions/file.h"

#include "cache.h"

using namespace generalFunctions;
using namespace cache;

constexpr char CONFIG_FILE_NAME[] = "config.txt";

int loadPage(int key);

int main()
{
    // std::string cacheConfig = readFile( CONFIG_FILE_NAME);

    cache_t<int> LRUcache(4);
    
    std::array<int, 12> requests = {1, 2, 3, 4, 1, 2, 5, 1, 2, 4, 3, 4};

    for( auto& req : requests)
    {
        LRUcache.lookupUpdate(req, loadPage);
    }

    std::cout << LRUcache.getStats().amountHits << " кол-во хитов\n";
    std::cout << LRUcache.getStats().amountRequests << " кол-во запросов\n";
}

int loadPage(int key)
{
    return key; 
}