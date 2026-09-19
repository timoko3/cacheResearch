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
    size_t line;
    size_t column;
};

class Lexer
{
    std::string buffer_;
    size_t pos_;
    size_t cur_line_;
    size_t cur_column_;
    std::string src_file_name_;

public:
    Lexer(const std::string& src_file_name);
    ~Lexer() = default;
    Token getNextToken();
    std::string getSrcFileName() const;

private:
    Token getIntNum();
    Token getIdentifier();
    void  skipSpaces();

};

#endif /* PARSER_H */