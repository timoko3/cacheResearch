#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "cache.h"
#include "cacheParser.h"
#include "cacheSystem.h"
#include "file.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

void printTokenArr(std::vector<lexer::Token> tokenArr);

int main(const int argc, const char** argv) {
    try {
        if (argc < 3) {
            throw std::invalid_argument("Args format: <configFile> <inputFile>");
        }

        std::vector<std::string> args(argv + 1, argv + argc);

        const std::filesystem::path configFile = argv[1];
        const std::filesystem::path inputFile = argv[2];

        std::string configStr = generalFunctions::readFile(configFile);
        std::string inputStr = generalFunctions::readFile(inputFile);

        lexer::Lexer configLexer(configStr);
        lexer::Lexer inputLexer(inputStr);

        // printTokenArr(configLexer.tokenize());
        // printTokenArr(inputLexer.tokenize());

        lexer::TokenStream tsConfig(configLexer.tokenize(), configFile.string());
        lexer::TokenStream tsInput(inputLexer.tokenize(), inputFile.string());

        parser::CacheParser<int> cacheParser;
        cacheParser.parseAll(tsConfig, tsInput);

        cache::CacheSystemParams cacheSysParams = cacheParser.getCacheSysParams();

        std::vector<int> requests = cacheParser.getReqList();

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
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    catch (...) {
        std::cerr << "Unknown error\n";
        return 1;
    }

    return 0;
}

void printTokenArr(std::vector<lexer::Token> tokenArr) {
    for (std::size_t i = 0; i < tokenArr.size(); ++i) {
        std::cout << "TYPE:    " << static_cast<int>(tokenArr[i].type) << '\n';

        if (tokenArr[i].type == lexer::TokenType::INT ||
            tokenArr[i].type == lexer::TokenType::END) {
            std::cout << "DATA:    " << std::get<int>(tokenArr[i].data) << '\n';
        }

        else {
            std::cout << "DATA:    " << std::get<std::string>(tokenArr[i].data) << '\n';
        }

        std::cout << "LINE:    " << tokenArr[i].line << '\n';
        std::cout << "COLUMN:  " << tokenArr[i].col << '\n';

        std::cout << "\n\n";
    }
}
