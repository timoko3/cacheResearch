#include <cstddef>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

namespace tests {

namespace {

lexer::TokenStream lexStream(const std::string& src, const std::string& fileName = "unknown_file") {
    lexer::Lexer lx(src, fileName);
    return lx.tokenize();
}

lexer::Token createIntToken(int value, std::size_t line, std::size_t col) {
    return {lexer::TokenType::INT, value, line, col};
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

TEST(TokenStreamConstructor, DefaultFileName) {
    lexer::TokenStream ts;

    EXPECT_EQ(ts.getFileName(), "unknown_file");
}

TEST(TokenStreamConstructor, CustomFileName) {
    lexer::TokenStream ts("test.txt");

    EXPECT_EQ(ts.getFileName(), "test.txt");
}

TEST(TokenStreamConstructor, EmptyStreamFailsVerify) {
    lexer::TokenStream ts;

    EXPECT_THROW(ts.verify(), std::invalid_argument);
}

TEST(TokenStreamConstructor, EndOnlyStreamVerifies) {
    lexer::TokenStream ts = lexStream("");

    EXPECT_NO_THROW(ts.verify());
}

TEST(TokenStreamValid, PeekDoesNotAdvance) {
    lexer::TokenStream ts = lexStream("5");

    const lexer::Token& first = ts.peekToken();
    const lexer::Token& second = ts.peekToken();

    ASSERT_EQ(&first, &second);
    EXPECT_EQ(first.type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(first.data), 5);
}

TEST(TokenStreamValid, MovePosReturnsCurrentAndAdvances) {
    lexer::TokenStream ts = lexStream("1 a");

    ts.movePos();

    const lexer::Token& token = ts.peekToken();

    ASSERT_EQ(token.type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(token.data), "a");
}

TEST(TokenStreamValid, MovePosStaysOnEnd) {
    lexer::TokenStream ts = lexStream("");

    const int numOfIter = 5;

    for (int i = 0; i < numOfIter; ++i) {
        ts.movePos();
        EXPECT_EQ(ts.peekToken().type, lexer::TokenType::END);
        EXPECT_TRUE(ts.atEnd());
    }
}

TEST(TokenStreamValid, NextIsEndToken) {
    lexer::TokenStream ts = lexStream("1");

    EXPECT_FALSE(ts.atEnd());

    ts.movePos();

    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, IsMatchTypeChecksCurrentToken) {
    lexer::TokenStream ts = lexStream("1");

    ASSERT_TRUE(ts.isMatchType(lexer::TokenType::INT));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::IDENTIFIER));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::END));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::ERROR));
    EXPECT_EQ(ts.peekToken().type, lexer::TokenType::INT);
}

TEST(TokenStreamValid, ExpectReturnIntValue) {
    lexer::TokenStream ts = lexStream("67");

    EXPECT_EQ(ts.expectInt("need int"), 67);
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ExpectReturnIdentValue) {
    lexer::TokenStream ts = lexStream("LRU");

    EXPECT_EQ(ts.expectIdent("need ident"), "LRU");
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ExpectTokenReturnsConsumedToken) {
    lexer::TokenStream ts = lexStream("\n    LRU");

    const lexer::Token& token = ts.expectToken(lexer::TokenType::IDENTIFIER, "need ident");

    EXPECT_EQ(token.type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(token.data), "LRU");
    EXPECT_EQ(token.line, 2u);
    EXPECT_EQ(token.col, 5u);
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ReadsFullSequence) {
    lexer::TokenStream ts = lexStream("2\nLRU\nLFU");

    EXPECT_EQ(ts.expectInt("levels"), 2);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(ts.expectIdent("strategy"), "LFU");
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamErrors, ExpectIntOnIdentifierThrows) {
    lexer::TokenStream ts = lexStream("LRU", "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectInt("need int"); }), "test.txt:1:1: need int");
}

TEST(TokenStreamErrors, ExpectThrowAtIdentOnInts) {
    lexer::TokenStream ts = lexStream("\n\n\n 7", "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectIdent("need ident"); }), "test.txt:4:2: need ident");
}

TEST(TokenStreamErrors, ExpectThrowAtTokenWrongType) {
    lexer::TokenStream ts = lexStream("7");

    EXPECT_THROW(ts.expectToken(lexer::TokenType::IDENTIFIER, "need ident"), std::runtime_error);
}

TEST(TokenStreamErrors, ExpectThrowAtEnd) {
    lexer::TokenStream ts = lexStream("\n\n   ", "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectInt("need int"); }), "test.txt:3:4: need int");
}

TEST(TokenStreamErrors, ExpectIntThrowsAfterLastToken) {
    lexer::TokenStream ts = lexStream("52");

    EXPECT_EQ(ts.expectInt("need int"), 52);
    EXPECT_THROW(ts.expectInt("need int"), std::runtime_error);
}

TEST(TokenStreamErrors, FailedExpectDoesNotAdvance) {
    lexer::TokenStream ts = lexStream("LFU");

    EXPECT_THROW(ts.expectInt("need int"), std::runtime_error);

    EXPECT_EQ(ts.peekToken().type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(ts.expectIdent("need ident"), "LFU");
}

TEST(TokenStreamErrors, FailAtTokenFormatsMessage) {
    lexer::TokenStream ts = lexStream("", "test.txt");

    EXPECT_EQ(errorText([&] { ts.failAtToken(createIntToken(1, 4, 9), "ERROR"); }),
              "test.txt:4:9: ERROR");
}

TEST(TokenStreamErrors, FailAtTokenUsesDefaultFileName) {
    lexer::TokenStream ts;

    EXPECT_EQ(errorText([&] { ts.failAtToken(createIntToken(1, 2, 3), "ERROR"); }),
              "unknown_file:2:3: ERROR");
}

TEST(LexerWithTokenStream, ReadsConfigText) {
    lexer::Lexer lx("3\nLRU\nLFU\nARC\n", "config.txt");
    lexer::TokenStream ts = lx.tokenize();

    EXPECT_EQ(ts.getFileName(), "config.txt");
    EXPECT_EQ(ts.expectInt("levels"), 3);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(ts.expectIdent("strategy"), "LFU");
    EXPECT_EQ(ts.expectIdent("strategy"), "ARC");
    EXPECT_TRUE(ts.atEnd());
}

TEST(LexerWithTokenStream, ReadsInputText) {
    lexer::Lexer lx("4\n8\n16\n5\n1 2 3 2 1\n", "input.txt");
    lexer::TokenStream ts = lx.tokenize();

    EXPECT_EQ(ts.expectInt("size"), 4);
    EXPECT_EQ(ts.expectInt("size"), 8);
    EXPECT_EQ(ts.expectInt("size"), 16);

    const int count = ts.expectInt("count");
    ASSERT_EQ(count, 5);

    std::vector<int> requests;
    for (int i = 0; i < count; ++i) {
        requests.push_back(ts.expectInt("request"));
    }

    EXPECT_EQ(requests, (std::vector<int>{1, 2, 3, 2, 1}));
    EXPECT_TRUE(ts.atEnd());
}

TEST(LexerWithTokenStream, LexerErrorIsReportedWithPosition) {
    lexer::Lexer lx("1 2 $", "input.txt");

    EXPECT_EQ(errorText([&] { lx.tokenize(); }), "input.txt:1:5: Unknown sym");
}

TEST(LexerWithTokenStream, WrongTokenTypeIsReportedWithPosition) {
    lexer::Lexer lx("2\nLRU 7\n", "config.txt");
    lexer::TokenStream ts = lx.tokenize();

    EXPECT_EQ(ts.expectInt("levels"), 2);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(errorText([&] { ts.expectIdent("strategy"); }), "config.txt:2:5: strategy");
}

} // namespace tests