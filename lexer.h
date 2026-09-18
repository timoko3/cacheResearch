#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <fstream>
#include <variant>
#include <string>

enum Type
{
    INT,
    END,
    ERROR,
    IDENTIFIER,
};

struct Token
{
    Type type;
    std::variant<int, std::string> value;
};

class Lexer
{
    std::string buffer_;
    size_t pos_;

public:
    Lexer(const std::string& src_file_name);
    ~Lexer() = default;
    Token getNextToken();

private:
    Token getIntNum();
    Token getIdentifier();
    void  skipSpaces();

};

#endif /* PARSER_H */