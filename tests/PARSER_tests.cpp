#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cacheParser.h"

namespace tests {

namespace {

class TempFile {
public:
    explicit TempFile(const std::string& content)
        : path_(std::filesystem::temp_directory_path() /
                ("cache_parser_test_" + std::to_string(nextId()) + ".txt")) {
        std::ofstream out(path_);
        if (!out) {
            throw std::runtime_error("Cannot create temporary parser test file");
        }

        out << content;
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    std::string path() const { return path_.string(); }

private:
    static std::size_t nextId() {
        static std::size_t id = 0;
        return id++;
    }

    std::filesystem::path path_;
};

} // namespace

TEST(CacheParserValid, ParsesConfigAndInput) {
    TempFile config("3\n"
                    "LRU\n"
                    "LFU\n"
                    "ARC\n");

    TempFile input("4\n"
                   "8\n"
                   "16\n"
                   "5\n"
                   "1 2 3 2 1\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseAll(configLexer, inputLexer));

    const auto params = parser.getCacheSysParams();

    ASSERT_EQ(params.levels.size(), 3u);

    EXPECT_EQ(params.levels[0].level, cache::cacheLevel_t::L1);
    EXPECT_EQ(params.levels[0].size, 4u);
    EXPECT_EQ(params.levels[0].strategy, cache::cacheEviction_t::C_LRU);

    EXPECT_EQ(params.levels[1].level, cache::cacheLevel_t::L2);
    EXPECT_EQ(params.levels[1].size, 8u);
    EXPECT_EQ(params.levels[1].strategy, cache::cacheEviction_t::C_LFU);

    EXPECT_EQ(params.levels[2].level, cache::cacheLevel_t::L3);
    EXPECT_EQ(params.levels[2].size, 16u);
    EXPECT_EQ(params.levels[2].strategy, cache::cacheEviction_t::C_ARC);

    EXPECT_EQ(parser.getReqList(), (std::vector<int>{1, 2, 3, 2, 1}));
}

TEST(CacheParserValid, ParsesEverySupportedStrategy) {
    const std::vector<std::pair<std::string, cache::cacheEviction_t>> strategies{
        {"LFU", cache::cacheEviction_t::C_LFU},
        {"LRU", cache::cacheEviction_t::C_LRU},
        {"LIRS", cache::cacheEviction_t::C_LIRS},
        {"2Q", cache::cacheEviction_t::C_2Q},
        {"ARC", cache::cacheEviction_t::C_ARC},
        {"REF", cache::cacheEviction_t::C_REF},
    };

    for (const auto& [name, expected] : strategies) {
        SCOPED_TRACE(name);

        TempFile config("1\n" + name + "\n");

        Lexer configLexer(config.path());

        CacheParser<int> parser;
        ASSERT_NO_THROW(parser.parseConfig(configLexer));

        const auto params = parser.getCacheSysParams();

        ASSERT_EQ(params.levels.size(), 1u);
        EXPECT_EQ(params.levels[0].level, cache::cacheLevel_t::L1);
        EXPECT_EQ(params.levels[0].strategy, expected);
    }
}

TEST(CacheParserValid, TwoQIsParsedAsIdentifier) {
    TempFile config("1\n"
                    "2Q\n");

    Lexer configLexer(config.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    const auto params = parser.getCacheSysParams();

    ASSERT_EQ(params.levels.size(), 1u);
    EXPECT_EQ(params.levels[0].strategy, cache::cacheEviction_t::C_2Q);
}

TEST(CacheParserValid, ZeroRequestsAreAllowed) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("8\n"
                   "0\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseAll(configLexer, inputLexer));

    EXPECT_TRUE(parser.getReqList().empty());

    const auto params = parser.getCacheSysParams();
    ASSERT_EQ(params.levels.size(), 1u);
    EXPECT_EQ(params.levels[0].size, 8u);
}

TEST(CacheParserValid, WhitespaceIsIgnored) {
    TempFile config("  2 \n"
                    "LRU\t2Q\n");

    TempFile input(" 4\t8 \n"
                   "3\n"
                   "1\t2 3\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseAll(configLexer, inputLexer));

    const auto params = parser.getCacheSysParams();
    ASSERT_EQ(params.levels.size(), 2u);

    EXPECT_EQ(params.levels[0].strategy, cache::cacheEviction_t::C_LRU);
    EXPECT_EQ(params.levels[1].strategy, cache::cacheEviction_t::C_2Q);
    EXPECT_EQ(params.levels[0].size, 4u);
    EXPECT_EQ(params.levels[1].size, 8u);

    EXPECT_EQ(parser.getReqList(), (std::vector<int>{1, 2, 3}));
}

TEST(CacheParserConfigErrors, LevelCountMustBeInt) {
    TempFile config("LRU\n");

    Lexer configLexer(config.path());
    CacheParser<int> parser;

    EXPECT_THROW(parser.parseConfig(configLexer), std::runtime_error);
}

TEST(CacheParserConfigErrors, UnknownStrategyIsRejected) {
    TempFile config("1\n"
                    "FIFO\n");

    Lexer configLexer(config.path());
    CacheParser<int> parser;

    EXPECT_THROW(parser.parseConfig(configLexer), std::runtime_error);
}

TEST(CacheParserConfigErrors, NotEnoughStrategiesAreRejected) {
    TempFile config("2\n"
                    "LRU\n");

    Lexer configLexer(config.path());
    CacheParser<int> parser;

    EXPECT_THROW(parser.parseConfig(configLexer), std::runtime_error);
}

TEST(CacheParserConfigErrors, ExtraStrategiesAreRejected) {
    TempFile config("1\n"
                    "LRU\n"
                    "LFU\n");

    Lexer configLexer(config.path());
    CacheParser<int> parser;

    EXPECT_THROW(parser.parseConfig(configLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, ParseInputBeforeConfigIsRejected) {
    TempFile input("8\n"
                   "0\n");

    Lexer inputLexer(input.path());
    CacheParser<int> parser;

    EXPECT_THROW(parser.parseInput(inputLexer), std::logic_error);
}

TEST(CacheParserInputErrors, LevelSizeMustBeInt) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("large\n"
                   "0\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, NotEnoughLevelSizesAreRejected) {
    TempFile config("2\n"
                    "LRU\n"
                    "LFU\n");

    TempFile input("8\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, RequestCountMustBeInt) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("8\n"
                   "many\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, NotEnoughRequestsAreRejected) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("8\n"
                   "3\n"
                   "10 20\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, RequestMustBeInt) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("8\n"
                   "2\n"
                   "10 bad\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserInputErrors, ExtraRequestsAreRejected) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile input("8\n"
                   "1\n"
                   "10 20\n");

    Lexer configLexer(config.path());
    Lexer inputLexer(input.path());

    CacheParser<int> parser;
    ASSERT_NO_THROW(parser.parseConfig(configLexer));

    EXPECT_THROW(parser.parseInput(inputLexer), std::runtime_error);
}

TEST(CacheParserReuse, ParseConfigReplacesPreviousLevels) {
    TempFile firstConfig("2\n"
                         "LRU\n"
                         "LFU\n");

    TempFile secondConfig("1\n"
                          "ARC\n");

    Lexer firstLexer(firstConfig.path());
    Lexer secondLexer(secondConfig.path());

    CacheParser<int> parser;

    ASSERT_NO_THROW(parser.parseConfig(firstLexer));
    ASSERT_NO_THROW(parser.parseConfig(secondLexer));

    const auto params = parser.getCacheSysParams();

    ASSERT_EQ(params.levels.size(), 1u);
    EXPECT_EQ(params.levels[0].level, cache::cacheLevel_t::L1);
    EXPECT_EQ(params.levels[0].strategy, cache::cacheEviction_t::C_ARC);
}

TEST(CacheParserReuse, ParseInputReplacesPreviousRequests) {
    TempFile config("1\n"
                    "LRU\n");

    TempFile firstInput("8\n"
                        "3\n"
                        "1 2 3\n");

    TempFile secondInput("16\n"
                         "2\n"
                         "9 10\n");

    Lexer configLexer(config.path());
    Lexer firstInputLexer(firstInput.path());
    Lexer secondInputLexer(secondInput.path());

    CacheParser<int> parser;

    ASSERT_NO_THROW(parser.parseConfig(configLexer));
    ASSERT_NO_THROW(parser.parseInput(firstInputLexer));
    ASSERT_NO_THROW(parser.parseInput(secondInputLexer));

    EXPECT_EQ(parser.getReqList(), (std::vector<int>{9, 10}));

    const auto params = parser.getCacheSysParams();
    ASSERT_EQ(params.levels.size(), 1u);
    EXPECT_EQ(params.levels[0].size, 16u);
}

} // namespace tests
