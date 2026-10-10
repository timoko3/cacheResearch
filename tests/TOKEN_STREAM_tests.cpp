#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "generalFunctions/lexer/lexer.h"
#include "generalFunctions/lexer/token.h"
#include "generalFunctions/lexer/token_stream.h"

namespace tests {

namespace {

lexer::Token createIntToken(int value, std::size_t line, std::size_t col) {
    return {lexer::TokenType::INT, value, line, col};
}

lexer::Token createIdentToken(const std::string& name, std::size_t line, std::size_t col) {
    return {lexer::TokenType::IDENTIFIER, name, line, col};
}

lexer::Token createErrorToken(const std::string& message, std::size_t line, std::size_t col) {
    return {lexer::TokenType::ERROR, message, line, col};
}

lexer::Token createEndToken(std::size_t line, std::size_t col) {
    return {lexer::TokenType::END, 0, line, col};
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

TEST(TokenStreamConstructor, AcceptsOnlyEnd) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};

    ASSERT_NO_THROW(lexer::TokenStream ts(tokenArr));
}

TEST(TokenStreamConstructor, DefaultFileName) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(ts.getFileName(), "unknown_file");
}

TEST(TokenStreamConstructor, CustomFileName) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};
    lexer::TokenStream ts(tokenArr, "test.txt");

    EXPECT_EQ(ts.getFileName(), "test.txt");
}

TEST(TokenStreamConstructor, EmptyTokenListIsRejected) {
    std::vector<lexer::Token> tokenArr;

    EXPECT_THROW(lexer::TokenStream ts(tokenArr), std::invalid_argument);
}

TEST(TokenStreamConstructor, ErrorAtLastTokenIsRejected) {
    std::vector<lexer::Token> tokenArr{createIntToken(1, 1, 1), createErrorToken("error", 1, 3)};

    EXPECT_THROW(lexer::TokenStream ts(tokenArr), std::runtime_error);
    EXPECT_EQ(errorText([&] { lexer::TokenStream ts(tokenArr, "test.txt"); }),
              "test.txt:1:3: Token with ERROR");
}

TEST(TokenStreamConstructor, MissingEndIsRejected) {
    std::vector<lexer::Token> tokenArr{createIntToken(1, 1, 1), createIdentToken("LRU", 1, 3)};

    EXPECT_THROW(lexer::TokenStream ts(tokenArr), std::runtime_error);
    EXPECT_EQ(errorText([&] { lexer::TokenStream ts(tokenArr, "test.txt"); }),
              "test.txt:1:3: Token list must end with END");
}

TEST(TokenStreamValid, PeekDoesNotAdvance) {
    std::vector<lexer::Token> tokenArr{createIntToken(5, 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    const lexer::Token& first = ts.peekToken();
    const lexer::Token& second = ts.peekToken();

    ASSERT_EQ(&first, &second);
    EXPECT_EQ(first.type, lexer::TokenType::INT);
    EXPECT_EQ(std::get<int>(first.data), 5);
}

TEST(TokenStreamValid, MovePosReturnsCurrentAndAdvances) {
    std::vector<lexer::Token> tokenArr{
        createIntToken(1, 1, 1), createIdentToken("a", 1, 3), createEndToken(1, 4)};
    lexer::TokenStream ts(tokenArr);

    ts.movePos();

    const lexer::Token& token = ts.peekToken();

    ASSERT_EQ(token.type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(token.data), "a");
}

TEST(TokenStreamValid, MovePosStaysOnEnd) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};
    lexer::TokenStream ts(tokenArr);

    const int numOfIter = 5;

    for (int i = 0; i < numOfIter; ++i) {
        ts.movePos();
        EXPECT_EQ(ts.peekToken().type, lexer::TokenType::END);
        EXPECT_TRUE(ts.atEnd());
    }
}

TEST(TokenStreamValid, NextIsEndToken) {
    std::vector<lexer::Token> tokenArr{createIntToken(1, 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_FALSE(ts.atEnd());

    ts.movePos();

    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, IsMatchTypeChecksCurrentToken) {
    std::vector<lexer::Token> tokenArr{createIntToken(1, 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    ASSERT_TRUE(ts.isMatchType(lexer::TokenType::INT));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::IDENTIFIER));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::END));
    EXPECT_FALSE(ts.isMatchType(lexer::TokenType::ERROR));
    EXPECT_EQ(ts.peekToken().type, lexer::TokenType::INT);
}

TEST(TokenStreamValid, ExpectReturnIntValue) {
    const int intValue = 67;
    std::vector<lexer::Token> tokenArr{createIntToken(intValue, 1, 1), createEndToken(1, 3)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(ts.expectInt("need int"), intValue);
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ExpectReturnIdentValue) {
    std::vector<lexer::Token> tokenArr{createIdentToken("LRU", 1, 1), createEndToken(1, 4)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(ts.expectIdent("need ident"), "LRU");
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ExpectTokenReturnsConsumedToken) {
    std::vector<lexer::Token> tokenArr{createIdentToken("LRU", 2, 5), createEndToken(2, 6)};
    lexer::TokenStream ts(tokenArr);

    const lexer::Token& Token = ts.expectToken(lexer::TokenType::IDENTIFIER, "need ident");

    EXPECT_EQ(Token.type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(Token.data), "LRU");
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamValid, ReadsFullSequence) {
    std::vector<lexer::Token> tokenArr{createIntToken(2, 1, 1),
                                       createIdentToken("LRU", 2, 1),
                                       createIdentToken("LFU", 3, 1),
                                       createEndToken(3, 4)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(ts.expectInt("levels"), 2);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(ts.expectIdent("strategy"), "LFU");
    EXPECT_TRUE(ts.atEnd());
}

TEST(TokenStreamErrors, ExpectIntOnIdentifierThrows) {
    std::vector<lexer::Token> tokenArr{createIdentToken("LRU", 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr, "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectInt("need int"); }), "test.txt:1:1: need int");
}

TEST(TokenStreamErrors, ExpectThrowAtIdentOnInts) {
    std::vector<lexer::Token> tokenArr{createIntToken(7, 4, 2), createEndToken(4, 3)};

    lexer::TokenStream ts(tokenArr, "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectIdent("need ident"); }), "test.txt:4:2: need ident");
}

TEST(TokenStreamErrors, ExpectThrowAtTokenWrongType) {
    std::vector<lexer::Token> tokenArr{createIntToken(7, 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_THROW(ts.expectToken(lexer::TokenType::IDENTIFIER, "need ident"), std::runtime_error);
}

TEST(TokenStreamErrors, ExpectThrowAtEnd) {
    std::vector<lexer::Token> tokenArr{createEndToken(3, 4)};
    lexer::TokenStream ts(tokenArr, "test.txt");

    EXPECT_EQ(errorText([&] { ts.expectInt("need int"); }), "test.txt:3:4: need int");
}

TEST(TokenStreamErrors, ExpecThrowtAtEndToken) {
    const int intValue = 52;
    std::vector<lexer::Token> tokenArr{createIntToken(intValue, 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(ts.expectInt("need int"), intValue);
    EXPECT_THROW(ts.expectInt("need int"), std::runtime_error);
}

TEST(TokenStreamErrors, FailedExpectDoesNotAdvance) {
    std::vector<lexer::Token> tokenArr{createIdentToken("LFU", 1, 1), createEndToken(1, 2)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_THROW(ts.expectInt("need int"), std::runtime_error);

    EXPECT_EQ(ts.peekToken().type, lexer::TokenType::IDENTIFIER);
    EXPECT_EQ(ts.expectIdent("need ident"), "LFU");
}

TEST(TokenStreamErrors, FailAtTokenFormatsMessage) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};
    lexer::TokenStream ts(tokenArr, "test.txt");

    EXPECT_EQ(errorText([&] { ts.failAtToken(createIntToken(1, 4, 9), "ERROR"); }),
              "test.txt:4:9: ERROR");
}

TEST(TokenStreamErrors, FailAtTokenUsesDefaultFileName) {
    std::vector<lexer::Token> tokenArr{createEndToken(1, 1)};
    lexer::TokenStream ts(tokenArr);

    EXPECT_EQ(errorText([&] { ts.failAtToken(createIntToken(1, 2, 3), "ERROR"); }),
              "unknown_file:2:3: ERROR");
}

TEST(LexerWithTokenStream, ReadsConfigText) {
    lexer::Lexer lx("3\nLRU\nLFU\nARC\n", "config.txt");
    lexer::TokenStream ts(lx.tokenize(), lx.getFileName());

    EXPECT_EQ(ts.expectInt("levels"), 3);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(ts.expectIdent("strategy"), "LFU");
    EXPECT_EQ(ts.expectIdent("strategy"), "ARC");
    EXPECT_TRUE(ts.atEnd());
}

TEST(LexerWithTokenStream, ReadsInputText) {
    lexer::Lexer lx("4\n8\n16\n5\n1 2 3 2 1\n", "input.txt");
    lexer::TokenStream ts(lx.tokenize(), lx.getFileName());

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
    const auto tokenArr = lx.tokenize();

    EXPECT_EQ(errorText([&] { lexer::TokenStream ts(tokenArr, lx.getFileName()); }),
              "input.txt:1:5: Token with ERROR");
}

TEST(LexerWithTokenStream, WrongTokenTypeIsReportedWithPosition) {
    lexer::Lexer lx("2\nLRU 7\n", "config.txt");
    lexer::TokenStream ts(lx.tokenize(), lx.getFileName());

    EXPECT_EQ(ts.expectInt("levels"), 2);
    EXPECT_EQ(ts.expectIdent("strategy"), "LRU");
    EXPECT_EQ(errorText([&] { ts.expectIdent("strategy"); }), "config.txt:2:5: strategy");
}

} // namespace tests
