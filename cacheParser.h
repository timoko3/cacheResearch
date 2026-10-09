#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "./generalFunctions/lexer/lexer.h"
#include "./generalFunctions/lexer/token.h"
#include "./generalFunctions/lexer/token_stream.h"
#include "cache/cache.h"
#include "cacheSystem.h"

namespace parser {

const int max_cache_level = 5;

template <typename keyT = int>
class CacheParser {
    cache::cacheSystemParams cacheSysParams_{};
    std::vector<keyT> reqList_;
    bool config_parsed_ = false;

public:
    CacheParser() {};

    void parseConfig(lexer::TokenStream& ts) {
        config_parsed_ = false;
        cacheSysParams_.levels.clear();

        const lexer::Token& level_token = ts.peekToken();

        const std::size_t count =
            static_cast<std::size_t>(ts.expectInt("Incorrect num of cache strategies"));

        if (count > max_cache_level) {
            ts.failAtToken(level_token, "Too many cache levels");
        }

        for (std::size_t level = 0; level < count; ++level) {
            if (!ts.isMatchType(lexer::TokenType::IDENTIFIER)) {
                ts.failAtToken(ts.peekToken(), "Not enough cache levels");
            }

            parseLevel(ts, static_cast<cache::cacheLevel_t>(level));
        }

        ts.expectToken(lexer::TokenType::END, "Extra data after cache levels");

        config_parsed_ = true;
    }

    void parseInput(lexer::TokenStream& ts) {
        if (!config_parsed_) {
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

    void parseAll(lexer::TokenStream& config_ts, lexer::TokenStream& input_ts) {
        parseConfig(config_ts);
        parseInput(input_ts);
    }

    const cache::cacheSystemParams& getCacheSysParams() const { return cacheSysParams_; }

    const std::vector<keyT>& getReqList() const { return reqList_; }

private:
    void parseLevel(lexer::TokenStream& ts, cache::cacheLevel_t level) {
        const lexer::Token& name_token =
            ts.expectToken(lexer::TokenType::IDENTIFIER, "Incorrect name of cache strategy");

        cacheSysParams_.levels.push_back({0, level, strategyFromToken(name_token, ts)});
    }

    cache::cacheEviction_t strategyFromToken(const lexer::Token& token,
                                             const lexer::TokenStream& ts) {
        const std::string& name = std::get<std::string>(token.data);

        if (name == "LFU")
            return cache::cacheEviction_t::C_LFU;
        if (name == "LRU")
            return cache::cacheEviction_t::C_LRU;
        if (name == "LIRS")
            return cache::cacheEviction_t::C_LIRS;
        if (name == "2Q")
            return cache::cacheEviction_t::C_2Q;
        if (name == "ARC")
            return cache::cacheEviction_t::C_ARC;
        if (name == "REF")
            return cache::cacheEviction_t::C_REF;

        ts.failAtToken(token, "Unknown cache strategy");
        return cache::cacheEviction_t::C_UNKNOWN;
    }
};

} // namespace parser
