#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "./generalFunctions/file.h"
#include "./generalFunctions/lexer/lexer.h"
#include "./generalFunctions/lexer/token.h"
#include "./generalFunctions/lexer/token_stream.h"
#include "cache.h"
#include "cacheParser.h"
#include "cacheSystem.h"

void printTokenArr(std::vector<lexer::Token> token_arr);

int slowGetPage(int key);

int main(const int argc, const char** argv) {
    try {
        if (argc < 3) {
            throw std::invalid_argument("Args format: <configFile> <inputFile>");
        }

        std::vector<std::string> args(argv + 1, argv + argc);

        const std::string configFile = argv[1];
        const std::string inputFile = argv[2];

        std::string config_str = generalFunctions::readFile(configFile);
        std::string input_str = generalFunctions::readFile(inputFile);

        lexer::Lexer configLexer(config_str);
        lexer::Lexer inputLexer(input_str);

        // printTokenArr(configLexer.tokenize());
        // printTokenArr(inputLexer.tokenize());

        lexer::TokenStream tsConfig(configLexer.tokenize(), configFile);
        lexer::TokenStream tsInput(inputLexer.tokenize(), inputFile);

        parser::CacheParser<int> cacheParser;
        cacheParser.parseAll(tsConfig, tsInput);

        cache::cacheSystemParams cacheSysParams = cacheParser.getCacheSysParams();

        std::vector<int> requests = cacheParser.getReqList();

        cache::CacheSystem<int> cacheSystem(cacheSysParams);

        for (int request : requests) {
            cacheSystem.lookupUpdate(request, slowGetPage);
        }

        auto stats = cacheSystem.getStats();

        std::cout << "Requests: " << stats.total.amountRequests << '\n';
        std::cout << "Hits:     " << stats.total.amountHits << '\n';
        std::cout << "Misses:   " << stats.total.amountMisses << '\n';
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

void printTokenArr(std::vector<lexer::Token> token_arr) {
    for (std::size_t i = 0; i < token_arr.size(); ++i) {
        std::cout << "TYPE:    " << static_cast<int>(token_arr[i].type) << '\n';

        if (token_arr[i].type == lexer::TokenType::INT ||
            token_arr[i].type == lexer::TokenType::END) {
            std::cout << "DATA:    " << std::get<int>(token_arr[i].data) << '\n';
        }

        else {
            std::cout << "DATA:    " << std::get<std::string>(token_arr[i].data) << '\n';
        }

        std::cout << "LINE:    " << token_arr[i].line << '\n';
        std::cout << "COLUMN:  " << token_arr[i].col << '\n';

        std::cout << "\n\n";
    }
}

int slowGetPage(int key) {
    return key;
}
