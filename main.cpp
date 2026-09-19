#include <iostream>
#include <string>
#include <vector>
#include <variant>

#include "lexer.h"
#include "cacheConfigParser.h"
#include "cache.h"
#include "cacheSystem.h"

const char* strategyName(cache::cacheEvictionType type);

int main(const int argc, const char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);


    const std::string configFile = argv[1];

    try
    {
        std::cout << "===== LEXER TEST =====\n";

        Lexer lexer(configFile);

        while (true)
        {
            Token token = lexer.getNextToken();

            std::cout
                << token.line
                << ":"
                << token.column
                << "  ";

            switch (token.type)
            {
                case INT:
                    std::cout
                        << "INT          "
                        << std::get<int>(token.value);
                    break;

                case IDENTIFIER:
                    std::cout
                        << "IDENTIFIER   "
                        << std::get<std::string>(token.value);
                    break;

                case END:
                    std::cout << "END";
                    break;

                case ERROR:
                    std::cout << "ERROR";
                    break;
            }

            std::cout << '\n';

            if (token.type == END)
            {
                break;
            }
        }


        std::cout << "\n===== PARSER TEST =====\n";


        Lexer parserLexer(configFile);

        CacheConfigParser parser(parserLexer);

        cache::cacheSystemParams params =
            parser.parseConfig();

        std::cout
            << "Levels count: "
            << params.levels.size()
            << '\n';

        for (std::size_t i = 0; i < params.levels.size(); ++i)
        {
            const auto& [size, level, strategy] =
                params.levels[i];

            std::cout
                << "L" << (i + 1)
                << ": "
                << strategyName(strategy)
                << '\n';
        }
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ERROR: "
            << error.what()
            << '\n';

        return 1;
    }

    return 0;
}

const char* strategyName(cache::cacheEvictionType type)
{
    switch (type)
    {
        case cache::C_LFU:
            return "LFU";

        case cache::C_LRU:
            return "LRU";

        case cache::C_LIRS:
            return "LIRS";

        case cache::C_2Q:
            return "2Q";

        case cache::C_ARC:
            return "ARC";

        case cache::C_REF:
            return "REF";

        default:
            return "UNKNOWN";
    }
}