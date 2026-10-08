#include <cstddef>
#include <limits>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "generalFunctions/lexer/lexer.h"
#include "generalFunctions/lexer/token.h"

namespace tests {

namespace {

lexer::Lexer::TokenArr_t lex(const std::string& source) {
    lexer::Lexer lx(source);
    return lx.tokenize();
}

bool hasEnd(const lexer::Lexer::TokenArr_t& tokens) {
    for (const auto& token : tokens) {
        if (token.type == lexer::TokenType::END) {
            return true;
        }
    }

    return false;
}

} // namespace

TEST(LexerValid, EmptyInputGivesOnlyEnd) {
    const auto tokens = lex("");

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, lexer::TokenType::END);
    EXPECT_EQ(tokens[0].line, 1u);
    EXPECT_EQ(tokens[0].col, 1u);
}

TEST(LexerValid, SpacesOnlyGiveOnlyEnd) {
    const auto tokens = lex("   \n \t");

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, lexer::TokenType::END);
    EXPECT_EQ(tokens[0].line, 2u);
    EXPECT_EQ(tokens[0].col, 3u);
}

TEST(LexerValid, ParsesInts) {
    const auto tokens = lex("1 2 3");

    ASSERT_EQ(tokens.size(), 4u);

    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(tokens[i].type, lexer::TokenType::INT);
        EXPECT_EQ(std::get<int>(tokens[i].data), static_cast<int>(i) + 1);
    }

    EXPECT_EQ(tokens[3].type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesIdentifiers) {
    const auto tokens = lex("LRU LFU");

    ASSERT_EQ(tokens.size(), 3u);

    EXPECT_EQ(tokens[0].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[0].data), "LRU");

    EXPECT_EQ(tokens[1].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "LFU");

    EXPECT_EQ(tokens[2].type, lexer::TokenType::END);
}

TEST(LexerValid, ParsesMixedIntsAndIdentifiers) {
    const auto tokens = lex("3 LRU 42 ARC");

    ASSERT_EQ(tokens.size(), 5u);

    EXPECT_EQ(tokens[0].type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokens[0].data), 3);

    EXPECT_EQ(tokens[1].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "LRU");

    EXPECT_EQ(tokens[2].type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokens[2].data), 42);

    EXPECT_EQ(tokens[3].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[3].data), "ARC");

    EXPECT_EQ(tokens[4].type, lexer::TokenType::END);
}

TEST(LexerValid, ZeroAndLeadingZeros) {
    const auto tokens = lex("0 007");

    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(std::get<int>(tokens[0].data), 0);
    EXPECT_EQ(std::get<int>(tokens[1].data), 7);
}

TEST(LexerValid, IntMaxIsInt) {
    const auto tokens = lex(std::to_string(std::numeric_limits<int>::max()));

    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokens[0].data), std::numeric_limits<int>::max());
}

TEST(LexerValid, IntWithoutTrailingSpace) {
    const auto tokens = lex("7");

    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(tokens[0].data), 7);
    EXPECT_EQ(tokens[1].type, lexer::TokenType::END);
}

TEST(LexerValid, DigitsFollowedByLettersAreIdentifier) {
    const auto tokens = lex("2Q 12abc");

    ASSERT_EQ(tokens.size(), 3u);

    EXPECT_EQ(tokens[0].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[0].data), "2Q");

    EXPECT_EQ(tokens[1].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "12abc");
}

TEST(LexerValid, IdentifierWithDigitsInside) {
    const auto tokens = lex("abc123 a1b2");

    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(std::get<std::string>(tokens[0].data), "abc123");
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "a1b2");
}

TEST(LexerValid, LongIdentifier) {
    const std::string name(100, 'a');
    const auto tokens = lex(name);

    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(std::get<std::string>(tokens[0].data), name);
}

TEST(LexerValid, TrailingSpacesDoNotAddTokens) {
    EXPECT_EQ(lex("1 2 ").size(), 3u);
    EXPECT_EQ(lex("1 2\n").size(), 3u);
    EXPECT_EQ(lex("1 2 \n\t ").size(), 3u);
}

TEST(LexerValid, DifferentWhitespaceSeparatesTokens) {
    const auto tokens = lex("1\t2\n3\r\n4  5");

    ASSERT_EQ(tokens.size(), 6u);

    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(std::get<int>(tokens[i].data), static_cast<int>(i) + 1);
    }
}

TEST(LexerValid, ConfigLikeInput) {
    const auto tokens = lex("3\nLRU\nLFU\nARC\n");

    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(std::get<int>(tokens[0].data), 3);
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "LRU");
    EXPECT_EQ(std::get<std::string>(tokens[2].data), "LFU");
    EXPECT_EQ(std::get<std::string>(tokens[3].data), "ARC");
    EXPECT_EQ(tokens[4].type, lexer::TokenType::END);
}

TEST(LexerPosition, TokensStoreStartPosition) {
    const auto tokens = lex("12 ab\n7");

    ASSERT_EQ(tokens.size(), 4u);

    EXPECT_EQ(tokens[0].line, 1u);
    EXPECT_EQ(tokens[0].col, 1u);

    EXPECT_EQ(tokens[1].line, 1u);
    EXPECT_EQ(tokens[1].col, 4u);

    EXPECT_EQ(tokens[2].line, 2u);
    EXPECT_EQ(tokens[2].col, 1u);

    EXPECT_EQ(tokens[3].type, lexer::TokenType::END);
    EXPECT_EQ(tokens[3].line, 2u);
    EXPECT_EQ(tokens[3].col, 2u);
}

TEST(LexerPosition, EmptyLinesIncreaseLine) {
    const auto tokens = lex("\n\n  x");

    ASSERT_EQ(tokens.size(), 2u);

    EXPECT_EQ(tokens[0].line, 3u);
    EXPECT_EQ(tokens[0].col, 3u);

    EXPECT_EQ(tokens[1].line, 3u);
    EXPECT_EQ(tokens[1].col, 4u);
}

TEST(LexerPosition, EndAfterTrailingNewline) {
    const auto tokens = lex("1\n");

    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].type, lexer::TokenType::END);
    EXPECT_EQ(tokens[1].line, 2u);
    EXPECT_EQ(tokens[1].col, 1u);
}

TEST(LexerErrors, UnknownSymbolAfterIdentifier) {
    const auto tokens = lex("abc$");

    ASSERT_EQ(tokens.size(), 2u);

    EXPECT_EQ(tokens[0].type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(tokens[0].data), "abc");

    EXPECT_EQ(tokens[1].type, lexer::TokenType::ERROR);
    EXPECT_EQ(tokens[1].line, 1u);
    EXPECT_EQ(tokens[1].col, 4u);
}

TEST(LexerErrors, UnknownSymbolAtStart) {
    const auto tokens = lex("$");

    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokens));
}

TEST(LexerErrors, LexingStopsAfterError) {
    const auto tokens = lex("ab ; cd 5");

    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokens));

    for (const auto& token : tokens) {
        if (token.type == lexer::TokenType::IDENTIFIER) {
            EXPECT_NE(std::get<std::string>(token.data), "cd");
        }

        EXPECT_NE(token.type, lexer::TokenType::INT);
    }
}

TEST(LexerErrors, ErrorMessageIsString) {
    const auto tokens = lex("ab;");

    ASSERT_FALSE(tokens.empty());
    ASSERT_EQ(tokens.back().type, lexer::TokenType::ERROR);
    EXPECT_TRUE(std::holds_alternative<std::string>(tokens.back().data));
}

TEST(LexerErrors, IntOverflowIsNotInt) {
    for (const std::string source : {"2147483648", "99999999999999999999"}) {
        SCOPED_TRACE(source);

        const auto tokens = lex(source);

        ASSERT_FALSE(tokens.empty());
        EXPECT_NE(tokens[0].type, lexer::TokenType::INT);
    }
}

TEST(LexerErrors, NonAsciiByteIsError) {
    const auto tokens = lex("ab\xFF");

    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.back().type, lexer::TokenType::ERROR);
    EXPECT_FALSE(hasEnd(tokens));
}

TEST(LexerFileName, DefaultName) {
    lexer::Lexer lx("1 2 3");

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

    const auto tokens = lx.tokenize();

    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(std::get<int>(tokens[0].data), 10);
    EXPECT_EQ(std::get<int>(tokens[1].data), 20);
}

TEST(LexerLifetime, WorksWithTemporaryString) {
    lexer::Lexer lx(std::string("5 LRU"));

    const auto tokens = lx.tokenize();

    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(std::get<int>(tokens[0].data), 5);
    EXPECT_EQ(std::get<std::string>(tokens[1].data), "LRU");
}

} // namespace tests
