#include <cstddef>
#include <limits>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "generalFunctions/lexer/lexer.h"
#include "generalFunctions/lexer/token.h"

namespace tests {

namespace {

bool hasEnd(const lexer::Lexer::TokenArr_t& tokenArr) {
    for (int i = 0; i < tokenArr.size(); ++i) {
        if (tokenArr.at(i).type == lexer::TokenType::END) {
            return true;
        }
    }

    return false;
}

} // namespace

TEST(LexerValid, EmptyInputGivesOnlyEnd) {
    lexer::Lexer lx("");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 1u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(0).line, 1u);
    EXPECT_EQ(tokenArr.at(0).col, 1u);
}

TEST(LexerValid, SpacesOnlyGiveOnlyEnd) {
    lexer::Lexer lx("   \n \t");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 1u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(0).line, 2u);
    EXPECT_EQ(tokenArr.at(0).col, 3u);
}

TEST(LexerValid, ParsesInts) {
    lexer::Lexer lx("1 2 3");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 4u);

    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(tokenArr.at(i).type, lexer::TokenType::INT);
        EXPECT_EQ(std::get<int>(tokenArr.at(i).data), static_cast<int>(i) + 1);
    }

    EXPECT_EQ(tokenArr.at(3).type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesIdentifiers) {
    lexer::Lexer lx("LRU LFU");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "LRU");

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LFU");

    EXPECT_EQ(tokenArr.at(2).type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesMixedIntsAndIdentifiers) {
    lexer::Lexer lx("3 LRU 42 ARC");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

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
    lexer::Lexer lx("0 007");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 0);
    EXPECT_EQ(std::get<int>(tokenArr.at(1).data), 7);
}

TEST(LexerValid, IntMaxIsInt) {
    lexer::Lexer lx(std::to_string(std::numeric_limits<int>::max()));
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), std::numeric_limits<int>::max());
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
}

TEST(LexerValid, IntWithoutTrailingSpace) {
    lexer::Lexer lx("7");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 7);
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
}

TEST(LexerValid, DigitsFollowedByLettersAreIdentifier) {
    lexer::Lexer lx("2Q 12abc");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "2Q");

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "12abc");
}

TEST(LexerValid, IdentifierWithDigitsInside) {
    lexer::Lexer lx("abc123 a1b2");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "abc123");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "a1b2");
}

TEST(LexerValid, LongIdentifier) {
    std::string longString = std::string(100, 'a');
    lexer::Lexer lx(longString);
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), longString);
}

TEST(LexerValid, TrailingSpacesDoNotAddTokens) {
    lexer::Lexer lx("1 2 ");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    EXPECT_EQ(tokenArr.size(), 3u);

    lx = lexer::Lexer("1 2\n");
    tokenArr = lx.tokenize();

    EXPECT_EQ(tokenArr.size(), 3u);

    lx = lexer::Lexer("1 2 \n\t ");
    tokenArr = lx.tokenize();

    EXPECT_EQ(tokenArr.size(), 3u);
}

TEST(LexerValid, DifferentWhitespaceSeparatesTokens) {
    lexer::Lexer lx("1\t2\n3\r\n4  5");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 6u);

    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(std::get<int>(tokenArr.at(i).data), static_cast<int>(i) + 1);
    }
}

TEST(LexerValid, ConfigLikeInput) {
    lexer::Lexer lx("3\nLRU\nLFU\nARC\n");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 5u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 3);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LRU");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(2).data), "LFU");
    EXPECT_EQ(std::get<std::string>(tokenArr.at(3).data), "ARC");
    EXPECT_EQ(tokenArr.at(4).type, lexer::TokenType::END);
}

TEST(LexerPosition, TokensStoreStartPosition) {
    lexer::Lexer lx("12 ab\n7");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

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
    lexer::Lexer lx("\n\n  x");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);

    EXPECT_EQ(tokenArr.at(0).line, 3u);
    EXPECT_EQ(tokenArr.at(0).col, 3u);

    EXPECT_EQ(tokenArr.at(1).line, 3u);
    EXPECT_EQ(tokenArr.at(1).col, 4u);
}

TEST(LexerPosition, EndAfterTrailingNewline) {
    lexer::Lexer lx("1\n");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);
    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::END);
    EXPECT_EQ(tokenArr.at(1).line, 2u);
    EXPECT_EQ(tokenArr.at(1).col, 1u);
}

TEST(LexerErrors, UnknownSymbolAfterIdentifier) {
    lexer::Lexer lx("abc$");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 2u);

    EXPECT_EQ(tokenArr.at(0).type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(0).data), "abc");

    EXPECT_EQ(tokenArr.at(1).type, lexer::TokenType::ERROR);
    EXPECT_EQ(tokenArr.at(1).line, 1u);
    EXPECT_EQ(tokenArr.at(1).col, 4u);
}

TEST(LexerErrors, UnknownSymbolAtStart) {
    lexer::Lexer lx("$");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_FALSE(tokenArr.empty());
    EXPECT_EQ(tokenArr.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokenArr));
}

TEST(LexerErrors, LexingStopsAfterError) {
    lexer::Lexer lx("ab ; cd 5");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_FALSE(tokenArr.empty());
    EXPECT_EQ(tokenArr.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokenArr));

    for (int i = 0; i < tokenArr.size(); ++i) {
        if (tokenArr.at(i).type == lexer::TokenType::IDENTIFIER) {
            EXPECT_NE(std::get<std::string>(tokenArr.at(i).data), "cd");
        }

        EXPECT_NE(tokenArr.at(i).type, lexer::TokenType::INT);
    }
}

TEST(LexerErrors, IntOverflowIsNotInt) {
    for (const std::string source : {"2147483648", "99999999999999999999"}) {
        SCOPED_TRACE(source);

        lexer::Lexer lx(source);
        lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

        ASSERT_FALSE(tokenArr.empty());
        EXPECT_NE(tokenArr.at(0).type, lexer::TokenType::INT);
    }
}

TEST(LexerErrors, NonAsciiByteIsError) {
    lexer::Lexer lx("ab\xFF");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_FALSE(tokenArr.empty());
    EXPECT_EQ(tokenArr.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokenArr));
}

TEST(LexerFileName, DefaultName) {
    lexer::Lexer lx("1 2 3");
    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    EXPECT_EQ(lx.getFileName(), "unknown_file");
}

TEST(LexerFileName, CustomName) {
    lexer::Lexer lx("1 2 3", "config.txt");

    EXPECT_EQ(lx.getFileName(), "config.txt");
}

TEST(LexerLifetime, LexerOwnsSourceBuffer) {
    std::string source = "10 20";
    lexer::Lexer lx(source);

    source = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";

    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 10);
    EXPECT_EQ(std::get<int>(tokenArr.at(1).data), 20);
}

TEST(LexerLifetime, WorksWithTemporaryString) {
    lexer::Lexer lx(std::string("5 LRU"));

    lexer::Lexer::TokenArr_t tokenArr = lx.tokenize();

    ASSERT_EQ(tokenArr.size(), 3u);
    EXPECT_EQ(std::get<int>(tokenArr.at(0).data), 5);
    EXPECT_EQ(std::get<std::string>(tokenArr.at(1).data), "LRU");
}

} // namespace tests
