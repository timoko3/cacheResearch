#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "cache/cache.h"
#include "cacheSystem.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

namespace parser {

const int maxCacheLevel = 5;

template <typename KeyT = int>
class CacheParser {
    cache::CacheSystemParams cacheSysParams_{};
    std::vector<KeyT> reqList_;
    bool configParsed_ = false;

public:
    CacheParser() {};

    void parseConfig(lexer::TokenStream& ts) {
        configParsed_ = false;
        cacheSysParams_.levels.clear();

        const lexer::Token& levelToken = ts.peekToken();

        const std::size_t count =
            static_cast<std::size_t>(ts.expectInt("Incorrect num of cache strategies"));

        if (count > maxCacheLevel) {
            ts.failAtToken(levelToken, "Too many cache levels");
        }

        for (std::size_t level = 0; level < count; ++level) {
            if (!ts.isMatchType(lexer::TokenType::IDENTIFIER)) {
                ts.failAtToken(ts.peekToken(), "Not enough cache levels");
            }

            parseLevel(ts, static_cast<cache::CacheLevel>(level));
        }

        ts.expectToken(lexer::TokenType::END, "Extra data after cache levels");

        configParsed_ = true;
    }

    void parseInput(lexer::TokenStream& ts) {
        if (!configParsed_) {
            throw std::logic_error("parseInput called before parseConfig");
        }

        reqList_.clear();

        for (auto& level : cacheSysParams_.levels) {
            level.size = ts.expectInt("Incorrect size of cache level");
        }

        const std::size_t count =
            static_cast<std::size_t>(ts.expectInt("Incorrect amount of requests"));

        for (std::size_t i = 0; i < count; ++i) {
            if (!ts.isMatchType(lexer::TokenType::INT)) {
                ts.failAtToken(ts.peekToken(), "Not enough requests");
            }

            reqList_.push_back(ts.expectInt("Incorrect request"));
        }

        ts.expectToken(lexer::TokenType::END,
                       "Extra data after requests check num of levels and num of requests");
    }

    void parseAll(lexer::TokenStream& configTs, lexer::TokenStream& inputTs) {
        parseConfig(configTs);
        parseInput(inputTs);
    }

    const cache::CacheSystemParams& getCacheSysParams() const { return cacheSysParams_; }

    const std::vector<KeyT>& getReqList() const { return reqList_; }

private:
    void parseLevel(lexer::TokenStream& ts, cache::CacheLevel level) {
        const lexer::Token& nameToken =
            ts.expectToken(lexer::TokenType::IDENTIFIER, "Incorrect name of cache strategy");

        cacheSysParams_.levels.push_back({0, level, strategyFromToken(nameToken, ts)});
    }

    cache::CacheEviction strategyFromToken(const lexer::Token& token,
                                           const lexer::TokenStream& ts) {
        const std::string& name = std::get<std::string>(token.data);

        if (name == "LFU")
            return cache::CacheEviction::C_LFU;
        if (name == "LRU")
            return cache::CacheEviction::C_LRU;
        if (name == "LIRS")
            return cache::CacheEviction::C_LIRS;
        if (name == "2Q")
            return cache::CacheEviction::C_2Q;
        if (name == "ARC")
            return cache::CacheEviction::C_ARC;
        if (name == "REF")
            return cache::CacheEviction::C_REF;

        ts.failAtToken(token, "Unknown cache strategy");
        return cache::CacheEviction::C_UNKNOWN;
    }
};

} // namespace parser
