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

    CacheLFU<int> LFUcache(3);
    
    std::array<int, 8> requests = {1, 2, 3, 3, 1, 2, 4, 2};

    for( auto& req : requests)
    {
        LFUcache.lookupUpdate(req, loadPage);
    }

    std::cout << LFUcache.getStats().amountHits << " кол-во хитов\n";
    std::cout << LFUcache.getStats().amountRequests << " кол-во запросов\n";
}

int loadPage(int key)
{
    return key; 
}