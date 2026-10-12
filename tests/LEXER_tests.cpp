#include <cstddef>
#include <limits>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

namespace tests {

namespace {

std::vector<lexer::Token> readAll(lexer::TokenStream& ts) {
    std::vector<lexer::Token> tokenArr;

    while (true) {
        tokenArr.push_back(ts.peekToken());

        if (ts.atEnd()) {
            break;
        }

        ts.movePos();
    }

    return tokenArr;
}

std::vector<lexer::Token> lexAll(const std::string& src) {
    lexer::Lexer lx(src);
    lexer::TokenStream ts = lx.tokenize();

    return readAll(ts);
}

template <typename F>
std::string errorText(F func) {
    try {
        func();
    } catch (const std::runtime_error& error) {
        return error.what();
    } catch (...) {
        return "Unknown error";
    }

    return "No Error";
}

} // namespace

TEST(LexerValid, EmptyInputGivesOnlyEnd) {
    const auto tokenArr = lexAll("");

    ASSERT_EQ(tokenArr.size(), 1u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(0).line, 1u);
    EXPECT_EQ(tokenArr.at(0).col, 1u);
}

TEST(LexerValid, SpacesOnlyGiveOnlyEnd) {
    const auto tokenArr = lexAll("   \n \t");

    ASSERT_EQ(tokenArr.size(), 1u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(0).line, 2u);
    EXPECT_EQ(tokenArr.at(0).col, 3u);
}

TEST(LexerValid, ParsesInts) {
    const auto tokenArr = lexAll("1 2 3");

    ASSERT_EQ(tokenArr.size(), 4u);

    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(tokenArr.at(i).type, lexer::TokenType::INT);
        EXPECT_EQ(std::get<int>(tokenArr.at(i).data), static_cast<int>(i) + 1);
    }

    EXPECT_EQ(tokenArr.at(3).type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesIdentifiers) {
    const auto tokenArr = lexAll("LRU LFU");

    ASSERT_EQ(tokenArr.size(), 3u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "LRU");

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LFU");

    EXPECT_EQ(tokenArr.at(2).type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesMixedIntsAndIdentifiers) {
    const auto tokenArr = lexAll("3 LRU 42 ARC");

    ASSERT_EQ(tokenArr.size(), 5u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 3);

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LRU");

    EXPECT_EQ(tokenArr.at(2).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(2).data), 42);

    EXPECT_EQ(tokenArr.at(3).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(3).data), "ARC");

    EXPECT_EQ(tokenArr.at(4).type, lexer::TokenType::END);
}

TEST(LexerValid, ZeroAndLeadingZeros) {
    const auto tokenArr = lexAll("0 007");

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 0);
    EXPECT_EQ(std::get<int>(tokenArr.at(1).data), 7);
}

TEST(LexerValid, IntMaxIsInt) {
    const auto tokenArr = lexAll(std::to_string(std::numeric_limits<int>::max()));

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), std::numeric_limits<int>::max());
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
}

TEST(LexerValid, IntWithoutTrailingSpace) {
    const auto tokenArr = lexAll("7");

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 7);
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
}

TEST(LexerValid, DigitsFollowedByLettersAreIdentifier) {
    const auto tokenArr = lexAll("2Q 12abc");

    ASSERT_EQ(tokenArr.size(), 3u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "2Q");

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "12abc");
}

TEST(LexerValid, IdentifierWithDigitsInside) {
    const auto tokenArr = lexAll("abc123 a1b2");

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "abc123");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "a1b2");
}

TEST(LexerValid, LongIdentifier) {
    const std::string longString(100, 'a');
    const auto tokenArr = lexAll(longString);

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), longString);
}

TEST(LexerValid, TrailingSpacesDoNotAddTokens) {
    for (const std::string source : {"1 2 ", "1 2\n", "1 2 \n\t "}) {
        SCOPED_TRACE(source);

        EXPECT_EQ(lexAll(source).size(), 3u);
    }
}

TEST(LexerValid, DifferentWhitespaceSeparatesTokens) {
    const auto tokenArr = lexAll("1\t2\n3\r\n4  5");

    ASSERT_EQ(tokenArr.size(), 6u);

    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(std::get<int>(tokenArr.at(i).data), static_cast<int>(i) + 1);
    }
}

TEST(LexerValid, ConfigLikeInput) {
    const auto tokenArr = lexAll("3\nLRU\nLFU\nARC\n");

    ASSERT_EQ(tokenArr.size(), 5u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 3);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LRU");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(2).data), "LFU");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(3).data), "ARC");
    EXPECT_EQ(tokenArr.at(4).type, lexer::TokenType::END);
}

TEST(LexerPosition, TokensStoreStartPosition) {
    const auto tokenArr = lexAll("12 ab\n7");

    ASSERT_EQ(tokenArr.size(), 4u);

    EXPECT_EQ(tokenArr.at(0).line, 1u);
    EXPECT_EQ(tokenArr.at(0).col, 1u);

    EXPECT_EQ(tokenArr.at(1).line, 1u);
    EXPECT_EQ(tokenArr.at(1).col, 4u);

    EXPECT_EQ(tokenArr.at(2).line, 2u);
    EXPECT_EQ(tokenArr.at(2).col, 1u);

    EXPECT_EQ(tokenArr.at(3).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(3).line, 2u);
    EXPECT_EQ(tokenArr.at(3).col, 2u);
}

TEST(LexerPosition, EmptyLinesIncreaseLine) {
    const auto tokenArr = lexAll("\n\n  x");

    ASSERT_EQ(tokenArr.size(), 2u);

    EXPECT_EQ(tokenArr.at(0).line, 3u);
    EXPECT_EQ(tokenArr.at(0).col, 3u);

    EXPECT_EQ(tokenArr.at(1).line, 3u);
    EXPECT_EQ(tokenArr.at(1).col, 4u);
}

TEST(LexerPosition, EndAfterTrailingNewline) {
    const auto tokenArr = lexAll("1\n");

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(1).line, 2u);
    EXPECT_EQ(tokenArr.at(1).col, 1u);
}

TEST(LexerErrors, UnknownSymbolAfterIdentifier) {
    EXPECT_EQ(errorText([] { lexAll("abc$"); }), "unknown_file:1:4: Unknown sym");
}

TEST(LexerErrors, UnknownSymbolAtStart) {
    EXPECT_EQ(errorText([] { lexAll("$"); }), "unknown_file:1:1: Unknown sym");
}

TEST(LexerErrors, DigitsThenSymbolIsError) {
    EXPECT_EQ(errorText([] { lexAll("12$"); }), "unknown_file:1:3: Unknown sym");
}

TEST(LexerErrors, LexingStopsAtFirstError) {
    EXPECT_EQ(errorText([] { lexAll("ab ; cd 5"); }), "unknown_file:1:4: Unknown sym");
}

TEST(LexerErrors, IntOverflowIsNotInt) {
    for (const std::string source : {"2147483648", "99999999999999999999"}) {
        SCOPED_TRACE(source);

        const auto tokenArr = lexAll(source);

        ASSERT_FALSE(tokenArr.empty());
        EXPECT_NE(tokenArr.at(0).type, lexer::TokenType::INT);
    }
}

TEST(LexerErrors, NonAsciiByteIsError) {
    EXPECT_EQ(errorText([] { lexAll("ab\xFF"); }), "unknown_file:1:3: Unknown sym");
}

TEST(LexerFileName, DefaultName) {
    lexer::Lexer lx("1 2 3");

    EXPECT_EQ(lx.getFileName(), "unknown_file");
}

TEST(LexerFileName, CustomName) {
    lexer::Lexer lx("1 2 3", "config.txt");

    EXPECT_EQ(lx.getFileName(), "config.txt");
}

TEST(LexerFileName, StreamKeepsName) {
    lexer::Lexer lx("1 2 3", "config.txt");
    lexer::TokenStream ts = lx.tokenize();

    EXPECT_EQ(ts.getFileName(), "config.txt");
}

TEST(LexerLifetime, LexerOwnsSourceBuffer) {
    std::string source = "10 20";
    lexer::Lexer lx(source);

    source = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";

    lexer::TokenStream ts = lx.tokenize();
    const auto tokenArr = readAll(ts);

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 10);
    EXPECT_EQ(std::get<int>(tokenArr.at(1).data), 20);
}

TEST(LexerLifetime, WorksWithTemporaryString) {
    lexer::Lexer lx(std::string("5 LRU"));
    lexer::TokenStream ts = lx.tokenize();

    const auto tokenArr = readAll(ts);

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 5);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LRU");
}

} // namespace tests