#include <iostream>
#include <fstream>  
#include <variant>
#include <string>
#include <filesystem>
#include <sstream>

#include "lexer.h"

using namespace std;

Lexer::Lexer(const string& src_file_name) 
    : pos_(0), src_file_name_(src_file_name),
      cur_line_(1), cur_column_(1)
{
    const auto size = filesystem::file_size(src_file_name);

    ifstream file(src_file_name, ios::binary);

    if (!file.is_open()) 
    {
        std::cerr << "Cannot open file: |" << src_file_name_ << "|" << '\n';
        throw std::runtime_error("Cannot open file");
    }

    buffer_.resize(size);

    file.read(buffer_.data(), size);
}

 std::string Lexer::getSrcFileName() const
 {
    return src_file_name_;
 }

Token Lexer::getIntNum()
{
    int value = 0;

    if (pos_ >= buffer_.size() || !isdigit(buffer_[pos_]))
    {
        return Token{ERROR, value, cur_line_, cur_column_};
    }

    int old_pos_ = pos_;

    while (pos_ < buffer_.size() && isdigit(buffer_[pos_]))
    {
        value = value * 10 + (buffer_[pos_] - '0');
        ++pos_;
        ++cur_column_;
    }

    if (!isspace(buffer_[pos_]))
    {
        pos_ = old_pos_;
        return Token{ERROR, value, cur_line_, cur_column_};
    }

    return Token{INT, value, cur_line_, cur_column_};
}

Token Lexer::getIdentifier()
{
    string value;

    while (pos_ < buffer_.size() && !isspace(buffer_[pos_]))
    {
        value.push_back(buffer_[pos_]);
        ++pos_;
        ++cur_column_;
    }

    return Token{IDENTIFIER, value, cur_line_, cur_column_};
}

void Lexer::skipSpaces()
{
    while (pos_ < buffer_.size() && isspace(buffer_[pos_]))
    {
        if (buffer_[pos_] == '\n')
        {
            ++cur_line_;
            cur_column_ = 0;
        }
        else
        {
            ++cur_column_;
        }

        ++pos_;
    }
}

Token Lexer::getNextToken()
{
    skipSpaces();

    if (pos_ >= buffer_.size())
        return Token{END, 0, cur_line_, cur_column_};

    Token nextToken = getIntNum();

    if (nextToken.type != ERROR)
        return nextToken;

    nextToken = getIdentifier();

    if (nextToken.type != ERROR)
        return nextToken;

    throw runtime_error("Unknown token type");
}