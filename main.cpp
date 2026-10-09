#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "./generalFunctions/lexer.h"
#include "cache.h"
#include "cacheParser.h"
#include "cacheSystem.h"

int main(const int argc, const char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    const std::string configFile = argv[1];
    const std::string inputFile = argv[2];

    Lexer configLexer(configFile);
    Lexer inputLexer(inputFile);

    CacheParser<int> Parser;

    Parser.parseAll(configLexer, inputLexer);

    cache::cacheSystemParams cacheSysParams = Parser.getCacheSysParams();
    std::vector<int> requests = Parser.getReqList();

    cache::CacheSystem<int> cacheSystem(cacheSysParams);

    int loadedPage = 0;
    auto slowGetPage = [&](int key) -> int& {
        loadedPage = key;
        return loadedPage;
    };

    for (int request : requests) {
        cacheSystem.lookupUpdate(request, slowGetPage);
    }

    auto stats = cacheSystem.getStats();

    std::cout << "Requests: " << stats.amountRequests << '\n';
    std::cout << "Hits:     " << stats.amountHits << '\n';
    std::cout << "Misses:   " << stats.amountMisses << '\n';

    return 0;
}
