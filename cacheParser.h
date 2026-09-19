#ifndef CACHE_CONFIG_PARSER
#define CACHE_CONFIG_PARSER

#include <string>
#include <vector>
#include <variant>
#include <stdexcept>

#include "./generalFunctions/lexer.h"
#include "cache.h"
#include "cacheSystem.h"

using namespace cache;

template <typename keyT = int>
class CacheParser
{
    size_t num_of_levels_;
    cache::cacheSystemParams cacheSysParams_;
    std::vector<keyT> requests_;
    Token currentToken_;

public:
    CacheParser() {};
    ~CacheParser() = default;

    void parseConfig(Lexer& configLexer)
    {
        currentToken_ = configLexer.getNextToken();

        if (currentToken_.type != INT)
        {
            grammarError(currentToken_, configLexer,
                       "Incorrect num of cache strategies");
        }

        num_of_levels_ = std::get<int>(currentToken_.value);

        for (size_t cur_level = L1; cur_level < num_of_levels_; cur_level++)
        {
            currentToken_ = configLexer.getNextToken();

            if (currentToken_.type == END)
            {
                grammarError(currentToken_, configLexer,
                             "Not enought levels in config file");
            }

            parseLevel(static_cast<cacheLevel>(cur_level), configLexer);
        }

        Token endToken = configLexer.getNextToken();

        if (endToken.type != END)
        {
            grammarError(endToken, configLexer,
                         "Check num of levels, not all have parsed yet");
        }

    }

    void parseInput(Lexer& configInput)
    {
        for (size_t level_it = 0; level_it < num_of_levels_; ++level_it)
        {
            currentToken_ = configInput.getNextToken();

            if (currentToken_.type != INT)
            {
                grammarError(currentToken_, configInput,
                             "Incorrect size of cache level");
            }

            cacheSysParams_.levels[level_it].size =
                std::get<int>(currentToken_.value);
        }

        currentToken_ = configInput.getNextToken();

        if (currentToken_.type != INT)
        {
            grammarError(currentToken_, configInput,
                         "Incorrect amount of requests");
        }

        size_t num_of_requests = std::get<int>(currentToken_.value);

        //std::cout << "NUM_OF_REQ " << num_of_requests << "\n\n"; 

        for (size_t request_it = 0; request_it < num_of_requests; ++request_it)
        {
            currentToken_ = configInput.getNextToken();

            // std::cout << "TTTTT " << currentToken_.type << "\n";
            // std::cout << "VVVVV " << std::get<int>(currentToken_.value) << "\n\n"; 
            
            if (currentToken_.type == END)
            {
                grammarError(currentToken_, configInput,
                             "Not enough requests, check num of requests in input file");
            }

            if (currentToken_.type != INT)
            {
                grammarError(currentToken_, configInput,
                             "Incorrect request type");
            }

            requests_.push_back(std::get<int>(currentToken_.value));
        }

        Token end_token = configInput.getNextToken();

        if (end_token.type != END)
        {
            grammarError(end_token, configInput,
                         "Check num of levels in config and amount of sizes in input files");
        }

    }

    void parseAll(Lexer& configLexer, Lexer& inputLexer)
    {
        parseConfig(configLexer);
        parseInput(inputLexer);
    }

    cache::cacheSystemParams getCacheSysParams() const
    {
        return cacheSysParams_;
    }

    std::vector<keyT> getReqList() const
    {
        return requests_;
    }

private:
    cache::cacheEvictionType parseStrategy(const std::string& strategy_name, 
                                           const Lexer& lexer) const
    {
        if (strategy_name == "LFU")  return C_LFU;
        if (strategy_name == "LRU")  return C_LRU;
        if (strategy_name == "LIRS") return C_LIRS;
        if (strategy_name == "2Q")   return C_2Q;
        if (strategy_name == "ARC")  return C_ARC;
        if (strategy_name == "REF")  return C_REF;

        grammarError(currentToken_,lexer,
                     "Unknown cache strategy");

        return C_UNKNOWN; // unreachable
    }

    void parseLevel(cacheLevel level, const Lexer& lexer)
    {
        if (currentToken_.type != IDENTIFIER)
        {
            grammarError(currentToken_, lexer,
                         "Incorrect name of cache strategy");
        }

        const std::string& strategy =
              std::get<std::string>(currentToken_.value);

        cacheSysParams_.levels.push_back({0, level,
                                          parseStrategy(strategy, lexer)});
    }

    void grammarError(const Token& token, const Lexer& lexer, 
                      const std::string& message) const
    {
        throw std::runtime_error(
            "Grammar error in " +
            lexer.getSrcFileName() + ":" +
            std::to_string(token.line) + ":" +
            std::to_string(token.column) + " " +
            message
        );
    }
};

#endif
