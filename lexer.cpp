#include <iostream>
#include <fstream>  
#include <variant>
#include <string>
#include <filesystem>
#include <sstream>

#include "lexer.h"

using namespace std;

Lexer::Lexer(const string& src_file_name) : pos_(0)
{
    const auto size = filesystem::file_size(src_file_name);

    ifstream file(src_file_name, ios::binary);

    if (!file.is_open()) 
    {
        throw runtime_error("Cannot open file");
    }

    buffer_.resize(size);

    file.read(buffer_.data(), size);
}

Token Lexer::getIntNum()
{
    int value = 0;

    if (pos_ >= buffer_.size() || !isdigit(buffer_[pos_]))
    {
        return Token{ERROR, value};
    }

    int old_pos_ = pos_;

    while (pos_ < buffer_.size() && isdigit(buffer_[pos_]))
    {
        value = value * 10 + (buffer_[pos_] - '0');
        ++pos_;
    }

    if (!isspace(buffer_[pos_]))
    {
        pos_ = old_pos_;
        return Token{ERROR, value};
    }

    return Token{INT, value};
}

Token Lexer::getIdentifier()
{
    string value;

    while (pos_ < buffer_.size() && !isspace(buffer_[pos_]))
    {
        value.push_back(buffer_[pos_]);
        ++pos_;
    }

    return Token{IDENTIFIER, value};
}

void Lexer::skipSpaces()
{
    while (pos_ < buffer_.size() && isspace(buffer_[pos_]))
    {
        ++pos_;
    }
}

Token Lexer::getNextToken()
{
    skipSpaces();

    if (pos_ >= buffer_.size())
        return Token{END, 0};

    Token nextToken = getIntNum();

    if (nextToken.type != ERROR)
        return nextToken;

    nextToken = getIdentifier();

    if (nextToken.type != ERROR)
        return nextToken;

    throw runtime_error("Unknown token type");
}