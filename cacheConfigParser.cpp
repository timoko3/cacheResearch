
#include <string>

#include "lexer.h"
#include "cache.h"
#include "cacheSystem.h"
#include "cacheConfigParser.h"

using namespace cache;

cacheSystemParams CacheConfigParser::parseConfig()
{
    currentToken_ = lexer_.getNextToken();

    if (currentToken_.type != INT)
    {
        grammarError(currentToken_, "Incorrect num of cache strategies");
    }

    int levelsCount = std::get<int>(currentToken_.value);

    for (int cur_level = L1; cur_level < levelsCount; cur_level++)
    {
        currentToken_ = lexer_.getNextToken();

        if (currentToken_.type == END)
        {
            grammarError(currentToken_, "Not enought levels in config file");
        }

        parseLevel(static_cast<cacheLevel>(cur_level));
    }

    if (lexer_.getNextToken().type != END)
    {
        grammarError(currentToken_, "Check num of levels, not all have parsed yet");
    }

    return cacheSysParams_;
}

cacheEvictionType CacheConfigParser::parseStrategy(const std::string& strategy_name) const
{
    if (strategy_name == "LFU")  return C_LFU;
    if (strategy_name == "LRU")  return C_LRU;
    if (strategy_name == "LIRS") return C_LIRS;
    if (strategy_name == "2Q")   return C_2Q;
    if (strategy_name == "ARC")  return C_ARC;
    if (strategy_name == "REF")  return C_REF;

    grammarError(currentToken_, "Unknown cache strategy");
    return C_UNKNOWN; //unreachable;
}

void CacheConfigParser::parseLevel(cacheLevel level)
{
    if(currentToken_.type != IDENTIFIER)
    {
        grammarError(currentToken_, "Incorrect name of cache strategy");
    }

    const std::string& strategy = std::get<std::string>(currentToken_.value);

    cacheSysParams_.levels.push_back({0, level, parseStrategy(strategy)});
}

void CacheConfigParser::grammarError(const Token& token,
                                     const std::string& message) const
{
    throw std::runtime_error("Grammar error in " + 
                             lexer_.getSrcFileName() + ":" +
                             std::to_string(token.line) + ":" + 
                             std::to_string(token.column) + " " +
                             message);
}