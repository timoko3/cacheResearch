#include <iostream>
#include <string>
#include <vector>
#include <variant>

#include "./generalFunctions/lexer_new.h"
#include "./generalFunctions/file.h"
//#include "cacheParser.h"
#include "cache.h"
#include "cacheSystem.h"

void printTokenArr(std::vector<lexer::Token> token_arr);

int slowGetPage(int key);

int main(const int argc, const char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    const std::string configFile = argv[1];
    const std::string inputFile = argv[2];

    std::string config_str = generalFunctions::readFile(configFile);

    lexer::Lexer ConfigLexer(config_str);

    printTokenArr(ConfigLexer.run());

    // CacheParser<int> Parser;

    // Parser.parseAll(configLexer, inputLexer);

    // cache::cacheSystemParams cacheSysParams = Parser.getCacheSysParams();
    // std::vector<int> requests = Parser.getReqList();

    // cache::CacheSystem<int> cacheSystem(cacheSysParams);

    // for (int request : requests)
    // {
    //     cacheSystem.lookupUpdate(request, slowGetPage);
    // }

    // auto stats = cacheSystem.getStats();

    // std::cout << "Requests: " << stats.total.amountRequests << '\n';
    // std::cout << "Hits:     " << stats.total.amountHits << '\n';
    // std::cout << "Misses:   " << stats.total.amountMisses << '\n';

    // return 0;
}

void printTokenArr(std::vector<lexer::Token> token_arr)
{
    for (std::size_t i = 0 ; i < token_arr.size(); ++i)
    {
        std::cout << "TYPE:    " << static_cast<int>(token_arr[i].type) << '\n';

        if (   token_arr[i].type == lexer::TokenType::INT 
            || token_arr[i].type == lexer::TokenType::END)
        {
            std::cout << "DATA:    " << std::get<int>(token_arr[i].data) << '\n';
        }

        else
        {
            std::cout << "DATA:    " << std::get<std::string>(token_arr[i].data) << '\n';
        }

        std::cout << "LINE:    " << token_arr[i].line << '\n';
        std::cout << "COLUMN:  " << token_arr[i].col  << '\n';

        std::cout << "\n\n";
    }
}

int slowGetPage(int key)
{
    return key;
}