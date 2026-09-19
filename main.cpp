#include <iostream>
#include <string>
#include <vector>
#include <variant>

#include "./generalFunctions/lexer.h"
#include "cacheParser.h"
#include "cache.h"
#include "cacheSystem.h"

int slowGetPage(int key);

int main(const int argc, const char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    const std::string configFile = argv[1];
    const std::string inputFile = argv[2];

    Lexer configLexer(configFile);
    Lexer inputLexer (inputFile);

    CacheParser<int> Parser;

    Parser.parseAll(configLexer, inputLexer);

    cache::cacheSystemParams cacheSysParams = Parser.getCacheSysParams();
    std::vector<int> requests = Parser.getReqList();

    cache::CacheSystem<int> cacheSystem(cacheSysParams);

    for (int request : requests)
    {
        cacheSystem.lookupUpdate(request, slowGetPage);
    }

    auto stats = cacheSystem.getStats();

    std::cout << "Requests: " << stats.total.amountRequests << '\n';
    std::cout << "Hits:     " << stats.total.amountHits << '\n';
    std::cout << "Misses:   " << stats.total.amountMisses << '\n';

    return 0;
}

int slowGetPage(int key)
{
    return key;
}