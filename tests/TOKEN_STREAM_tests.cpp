#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

namespace tests {

namespace {

using lexer::Token;
using lexer::TokenStream;
using lexer::TokenType;

Token intToken(int value, std::size_t line, std::size_t col) {
    return {TokenType::INT, value, line, col};
}

Token identToken(const std::string& name, std::size_t line, std::size_t col) {
    return {TokenType::IDENTIFIER, name, line, col};
}

Token errorToken(const std::string& message, std::size_t line, std::size_t col) {
    return {TokenType::ERROR, message, line, col};
}

Token endToken(std::size_t line, std::size_t col) {
    return {TokenType::END, 0, line, col};
}

template <typename Action>
std::string errorText(Action action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }

    return "";
}

} // namespace

TEST(TokenStreamConstructor, AcceptsOnlyEnd) {
    std::vector<Token> tokens{endToken(1, 1)};

    ASSERT_NO_THROW(TokenStream stream(tokens));
}

TEST(TokenStreamConstructor, DefaultFileName) {
    std::vector<Token> tokens{endToken(1, 1)};
    TokenStream stream(tokens);

    EXPECT_EQ(stream.getFileName(), "unknown_file");
}

TEST(TokenStreamConstructor, CustomFileName) {
    std::vector<Token> tokens{endToken(1, 1)};
    TokenStream stream(tokens, "main.txt");

    EXPECT_EQ(stream.getFileName(), "main.txt");
}

TEST(TokenStreamConstructor, EmptyTokenListIsRejected) {
    std::vector<Token> tokens;

    EXPECT_THROW(TokenStream stream(tokens), std::invalid_argument);
}

TEST(TokenStreamConstructor, ErrorAsLastTokenIsRejected) {
    std::vector<Token> tokens{intToken(1, 1, 1), errorToken("bad", 1, 3)};

    EXPECT_THROW(TokenStream stream(tokens), std::runtime_error);
    EXPECT_EQ(errorText([&] { TokenStream stream(tokens, "main.txt"); }),
              "main.txt:1:3: Token with ERROR");
}

TEST(TokenStreamConstructor, MissingEndIsRejected) {
    std::vector<Token> tokens{intToken(1, 1, 1), identToken("LRU", 1, 3)};

    EXPECT_THROW(TokenStream stream(tokens), std::runtime_error);
    EXPECT_EQ(errorText([&] { TokenStream stream(tokens, "main.txt"); }),
              "main.txt:1:3: Token list must end with END");
}

TEST(TokenStreamValid, PeekDoesNotAdvance) {
    std::vector<Token> tokens{intToken(5, 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    const Token& first = stream.peekToken();
    const Token& second = stream.peekToken();

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(first.type, TokenType::INT);
    EXPECT_EQ(std::get<int>(first.data), 5);
}

TEST(TokenStreamValid, MovePosReturnsCurrentAndAdvances) {
    std::vector<Token> tokens{intToken(1, 1, 1), identToken("a", 1, 3), endToken(1, 4)};
    TokenStream stream(tokens);

    const Token& first = stream.movePos();
    EXPECT_EQ(first.type, TokenType::INT);
    EXPECT_EQ(std::get<int>(first.data), 1);
    EXPECT_EQ(stream.peekToken().type, TokenType::IDENTIFIER);

    const Token& second = stream.movePos();
    EXPECT_EQ(second.type, TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(second.data), "a");
    EXPECT_EQ(stream.peekToken().type, TokenType::END);
}

TEST(TokenStreamValid, MovePosStaysOnEnd) {
    std::vector<Token> tokens{endToken(1, 1)};
    TokenStream stream(tokens);

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(stream.movePos().type, TokenType::END);
        EXPECT_TRUE(stream.atEnd());
    }
}

TEST(TokenStreamValid, AtEndOnlyOnEndToken) {
    std::vector<Token> tokens{intToken(1, 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    EXPECT_FALSE(stream.atEnd());

    stream.movePos();

    EXPECT_TRUE(stream.atEnd());
}

TEST(TokenStreamValid, IsMatchTypeChecksCurrentToken) {
    std::vector<Token> tokens{intToken(1, 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    EXPECT_TRUE(stream.isMatchType(TokenType::INT));
    EXPECT_FALSE(stream.isMatchType(TokenType::IDENTIFIER));
    EXPECT_FALSE(stream.isMatchType(TokenType::END));
    EXPECT_EQ(stream.peekToken().type, TokenType::INT);
}

TEST(TokenStreamValid, ExpectIntReturnsValueAndAdvances) {
    std::vector<Token> tokens{intToken(42, 1, 1), endToken(1, 3)};
    TokenStream stream(tokens);

    EXPECT_EQ(stream.expectInt("need int"), 42);
    EXPECT_TRUE(stream.atEnd());
}

TEST(TokenStreamValid, ExpectIdentReturnsNameAndAdvances) {
    std::vector<Token> tokens{identToken("LRU", 1, 1), endToken(1, 4)};
    TokenStream stream(tokens);

    EXPECT_EQ(stream.expectIdent("need ident"), "LRU");
    EXPECT_TRUE(stream.atEnd());
}

TEST(TokenStreamValid, ExpectTokenReturnsConsumedToken) {
    std::vector<Token> tokens{identToken("x", 2, 5), endToken(2, 6)};
    TokenStream stream(tokens);

    const Token& token = stream.expectToken(TokenType::IDENTIFIER, "need ident");

    EXPECT_EQ(token.type, TokenType::IDENTIFIER);
    EXPECT_EQ(std::get<std::string>(token.data), "x");
    EXPECT_EQ(token.line, 2u);
    EXPECT_EQ(token.col, 5u);
    EXPECT_TRUE(stream.atEnd());
}

TEST(TokenStreamValid, IdentReferenceStaysValidAfterMovePos) {
    std::vector<Token> tokens{identToken("very_long_identifier_name_not_in_sso", 1, 1),
                              identToken("next", 1, 38),
                              endToken(1, 42)};
    TokenStream stream(tokens);

    const std::string& name = stream.expectIdent("need ident");
    stream.movePos();

    EXPECT_EQ(name, "very_long_identifier_name_not_in_sso");
}

TEST(TokenStreamValid, ReadsFullSequence) {
    std::vector<Token> tokens{
        intToken(2, 1, 1), identToken("LRU", 2, 1), identToken("LFU", 3, 1), endToken(3, 4)};
    TokenStream stream(tokens);

    EXPECT_EQ(stream.expectInt("levels"), 2);
    EXPECT_EQ(stream.expectIdent("strategy"), "LRU");
    EXPECT_EQ(stream.expectIdent("strategy"), "LFU");
    EXPECT_TRUE(stream.atEnd());
}

TEST(TokenStreamErrors, ExpectIntOnIdentifierThrows) {
    std::vector<Token> tokens{identToken("a", 1, 1), endToken(1, 2)};
    TokenStream stream(tokens, "main.txt");

    EXPECT_EQ(errorText([&] { stream.expectInt("need int"); }), "main.txt:1:1: need int");
}

TEST(TokenStreamErrors, ExpectIdentOnIntThrows) {
    std::vector<Token> tokens{intToken(7, 4, 2), endToken(4, 3)};
    TokenStream stream(tokens, "main.txt");

    EXPECT_EQ(errorText([&] { stream.expectIdent("need ident"); }), "main.txt:4:2: need ident");
}

TEST(TokenStreamErrors, ExpectTokenWrongTypeThrows) {
    std::vector<Token> tokens{intToken(7, 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    EXPECT_THROW(stream.expectToken(TokenType::IDENTIFIER, "need ident"), std::runtime_error);
}

TEST(TokenStreamErrors, ExpectAtEndThrows) {
    std::vector<Token> tokens{endToken(3, 4)};
    TokenStream stream(tokens, "main.txt");

    EXPECT_EQ(errorText([&] { stream.expectInt("need int"); }),
              "main.txt:3:4: END of token arr did not expect");
}

TEST(TokenStreamErrors, ExpectAfterLastTokenThrows) {
    std::vector<Token> tokens{intToken(1, 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    EXPECT_EQ(stream.expectInt("need int"), 1);
    EXPECT_THROW(stream.expectInt("need int"), std::runtime_error);
}

TEST(TokenStreamErrors, FailedExpectDoesNotAdvance) {
    std::vector<Token> tokens{identToken("a", 1, 1), endToken(1, 2)};
    TokenStream stream(tokens);

    EXPECT_THROW(stream.expectInt("need int"), std::runtime_error);

    EXPECT_EQ(stream.peekToken().type, TokenType::IDENTIFIER);
    EXPECT_EQ(stream.expectIdent("need ident"), "a");
}

TEST(TokenStreamErrors, FailAtTokenFormatsMessage) {
    std::vector<Token> tokens{endToken(1, 1)};
    TokenStream stream(tokens, "main.txt");

    EXPECT_EQ(errorText([&] { stream.failAtToken(intToken(1, 4, 9), "boom"); }),
              "main.txt:4:9: boom");
}

TEST(TokenStreamErrors, FailAtTokenUsesDefaultFileName) {
    std::vector<Token> tokens{endToken(1, 1)};
    TokenStream stream(tokens);

    EXPECT_EQ(errorText([&] { stream.failAtToken(intToken(1, 2, 3), "boom"); }),
              "unknown_file:2:3: boom");
}

TEST(LexerWithTokenStream, ReadsConfigText) {
    lexer::Lexer lx("3\nLRU\nLFU\nARC\n", "config.txt");
    TokenStream stream(lx.tokenize(), lx.getFileName());

    EXPECT_EQ(stream.expectInt("levels"), 3);
    EXPECT_EQ(stream.expectIdent("strategy"), "LRU");
    EXPECT_EQ(stream.expectIdent("strategy"), "LFU");
    EXPECT_EQ(stream.expectIdent("strategy"), "ARC");
    EXPECT_TRUE(stream.atEnd());
}

TEST(LexerWithTokenStream, ReadsInputText) {
    lexer::Lexer lx("4\n8\n16\n5\n1 2 3 2 1\n", "input.txt");
    TokenStream stream(lx.tokenize(), lx.getFileName());

    EXPECT_EQ(stream.expectInt("size"), 4);
    EXPECT_EQ(stream.expectInt("size"), 8);
    EXPECT_EQ(stream.expectInt("size"), 16);

    const int count = stream.expectInt("count");
    ASSERT_EQ(count, 5);

    std::vector<int> requests;
    for (int i = 0; i < count; ++i) {
        requests.push_back(stream.expectInt("request"));
    }

    EXPECT_EQ(requests, (std::vector<int>{1, 2, 3, 2, 1}));
    EXPECT_TRUE(stream.atEnd());
}

TEST(LexerWithTokenStream, LexerErrorIsReportedWithPosition) {
    lexer::Lexer lx("1 2 $", "input.txt");
    const auto tokens = lx.tokenize();

    EXPECT_EQ(errorText([&] { TokenStream stream(tokens, lx.getFileName()); }),
              "input.txt:1:5: Token with ERROR");
}

TEST(LexerWithTokenStream, WrongTokenTypeIsReportedWithPosition) {
    lexer::Lexer lx("2\nLRU 7\n", "config.txt");
    TokenStream stream(lx.tokenize(), lx.getFileName());

    EXPECT_EQ(stream.expectInt("levels"), 2);
    EXPECT_EQ(stream.expectIdent("strategy"), "LRU");
    EXPECT_EQ(errorText([&] { stream.expectIdent("strategy"); }), "config.txt:2:5: strategy");
}

} // namespace tests
