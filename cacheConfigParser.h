#ifndef CACHE_CONFIG_PARSER
#define CACHE_CONFIG_PARSER

#include <string>

#include "lexer.h"
#include "cache.h"
#include "cacheSystem.h"

class CacheConfigParser
{
    Lexer& lexer_;
    cache::cacheSystemParams cacheSysParams_;
    Token currentToken_;

public:
    CacheConfigParser(Lexer& lexer) : lexer_(lexer) {};
    ~CacheConfigParser() = default;

    cache::cacheSystemParams parseConfig();

private:
    cache::cacheEvictionType parseStrategy(const std::string& strategy_name) const;

    void parseLevel(cache::cacheLevel level);

    void grammarError(const Token& token,
                      const std::string& message) const;
};

#endif