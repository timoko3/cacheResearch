#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <array>
#include <vector>

#include "generalFunctions/file.h"

#include "cache.h"

using namespace generalFunctions;
using namespace cache;

constexpr char CONFIG_FILE_NAME[] = "config.txt";

int loadPage(int key);

int main()
{
    // std::string cacheConfig = readFile( CONFIG_FILE_NAME);

std::list<int> requests = {
    1, 2, 3,
    1, 2,
    4,
    1, 2,
    3, 4
};
// hits = 5
    CacheREF<int> REFcache(3, requests);

    for( auto& req : requests)
    {
        REFcache.lookupUpdate(req, loadPage);
    }

    std::cout << REFcache.getStats().amountHits << " кол-во хитов\n";
    std::cout << REFcache.getStats().amountRequests << " кол-во запросов\n";
}

int loadPage(int key)
{
    return key; 
}