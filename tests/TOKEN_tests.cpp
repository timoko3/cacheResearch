#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "generalFunctions/lexer/token.h"

namespace tests {

TEST(TokenValid, IntToken) {
    lexer::Token token{lexer::TokenType::INT, 42, 3, 7};

    EXPECT_EQ(token.type, lexer::TokenType::INT);
    ASSERT_TRUE(std::holds_alternative<int>(token.data));
    EXPECT_EQ(std::get<int>(token.data), 42);
    EXPECT_EQ(token.line, 3u);
    EXPECT_EQ(token.col, 7u);
}

TEST(TokenValid, IdentifierToken) {
    lexer::Token token{lexer::TokenType::IDENTIFIER, std::string("LRU"), 2, 5};

    EXPECT_EQ(token.type, lexer::TokenType::IDENTIFIER);
    ASSERT_TRUE(std::holds_alternative<std::string>(token.data));
    EXPECT_EQ(std::get<std::string>(token.data), "LRU");
    EXPECT_EQ(token.line, 2u);
    EXPECT_EQ(token.col, 5u);
}

TEST(TokenValid, ErrorTokenHoldsMessage) {
    lexer::Token token{lexer::TokenType::ERROR, std::string("Unknown sym"), 1, 4};

    EXPECT_EQ(token.type, lexer::TokenType::ERROR);
    ASSERT_TRUE(std::holds_alternative<std::string>(token.data));
    EXPECT_EQ(std::get<std::string>(token.data), "Unknown sym");
}

TEST(TokenValid, EndTokenHoldsZero) {
    lexer::Token token{lexer::TokenType::END, 0, 1, 1};

    EXPECT_EQ(token.type, lexer::TokenType::END);
    ASSERT_TRUE(std::holds_alternative<int>(token.data));
    EXPECT_EQ(std::get<int>(token.data), 0);
}

TEST(TokenValid, CopyIsIndependent) {
    lexer::Token original{lexer::TokenType::IDENTIFIER, std::string("LRU"), 1, 1};
    lexer::Token copy = original;

    std::get<std::string>(original.data) = "LFU";

    EXPECT_EQ(std::get<std::string>(copy.data), "LRU");
    EXPECT_EQ(std::get<std::string>(original.data), "LFU");
}

TEST(TokenErrors, GetWrongAlternativeThrows) {
    lexer::Token intToken{lexer::TokenType::INT, 1, 1, 1};
    lexer::Token identToken{lexer::TokenType::IDENTIFIER, std::string("a"), 1, 1};

    EXPECT_THROW(std::get<std::string>(intToken.data), std::bad_variant_access);
    EXPECT_THROW(std::get<int>(identToken.data), std::bad_variant_access);
}

} // namespace tests
