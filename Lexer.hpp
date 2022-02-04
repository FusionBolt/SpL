//
// Created by fusionbolt on 2020/8/13.
//

#ifndef INTERPRETER_LEXER_HPP
#define INTERPRETER_LEXER_HPP

#include <string>
#include <vector>

namespace In
{
    class Token
    {
    public:
        enum Type
        {
            NumLiteral,
            Char,
            Operator,
            StringLiteral,
            KeyWord,
            Identifier,
            Symbol
        };

        [[nodiscard]] std::string TypeToStr(Type type) const
        {
            switch (type)
            {
                case NumLiteral:
                    return "NumLiteral";
                case Char:
                    return "Char";
                case Operator:
                    return "Operator";
                case StringLiteral:
                    return "StringLiteral";
                case KeyWord:
                    return "KeyWord";
                case Identifier:
                    return "Identifier";
                case Symbol:
                    return "Symbol";
            }
        }

        Token(Type type, std::string value) : _type(type), _value(std::move(value))
        {
        }

        Token(Type type, char value) : _type(type)
        { _value = value; }

        void Output() const
        {
            std::cout << "type:" << TypeToStr(_type)
                      << " value:" << _value
                      << std::endl;
        }

        Type GetType() const
        { return _type; }

        std::string GetValue() const
        { return _value; }

    private:
        Type _type;
        std::string _value;
    };

    void Boom(std::string errorInfo = "")
    {
        std::cout << "Boom" << std::endl;
        exit(-1);
    }

    bool IsLetter(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    bool IsOperator(char c)
    {
        // TODO: == != ... ++ --
        return std::string_view("+-*/%=!|&><~").find(c) != std::string::npos;
    }

    bool IsUnaryOp(char c)
    {
        return c == '-' || c == '~';
    }

    bool IsSymbol(char c)
    {
        return std::string_view("{}();:,").find(c) != std::string::npos;
    }

    bool IsNum(char c)
    { return (c >= '0' && c <= '9'); }

    std::vector keyWords = {"char", "int", "bool", "void", "float",
                            "if", "else", "while", "for", "continue",
                            "break", "switch", "case", "default", "return",
                            "true", "false"};

    bool IsKeyWord(std::string_view s)
    {
        return std::find(keyWords.begin(), keyWords.end(), s) != keyWords.end();
    }

    bool IsTypeKeyWord(std::string_view s)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            if (keyWords[i] == s)
            {
                return true;
            }
        }
        return false;
    }

    bool IsBlank(char c)
    {
        return std::string_view("\r\n\t ").find(c) != std::string::npos;
    }

    void skip(std::string_view str, char c, size_t &i)
    {
        while (str[i] != c && i < str.size())
        {
            ++i;
        }
    }

    std::vector<Token> Tokenize(std::string_view str)
    {
        std::vector<Token> tokens;
        size_t i = 0;
        while (i < str.size())
        {
            while (IsBlank(str[i]))
            {
                ++i;
            }
            switch (str[i])
            {
                case '"':
                {
                    ++i;
                    auto first = i;
                    skip(str, '"', i);
                    auto s = std::string(str.substr(first, i - first));
                    tokens.emplace_back(Token::StringLiteral, s);
                    ++i;
                    break;
                }
                case '\'':
                {
                    if (str[i + 2] != '\'')
                    {
                        Boom();
                    }
                    tokens.emplace_back(Token::Char, str[i + 1]);
                    i += 2;
                    break;
                }
                case '/':
                {
                    ++i;
                    if (str[i] != '/')
                    {
                        Boom();
                    }
                    // TODO:\r \n \r\n
                    skip(str, '\n', i);
                }
                case '#':
                {
                    skip(str, '\n', i);
                }
                default:
                {
                    if (IsLetter(str[i]))
                    {
                        auto first = i;
                        while (str[i] != ' ' && !IsSymbol(str[i]) && !IsOperator(str[i]))
                        {
                            ++i;
                        }
                        auto s = std::string(str.substr(first, i - first));
                        if (IsKeyWord(s))
                        {
                            tokens.emplace_back(Token::KeyWord, s);
                        }
                        else
                        {
                            tokens.emplace_back(Token::Identifier, s);
                        }
                    }
                    else if (IsOperator(str[i]))
                    {
                        // TODO: == != >= <=
                        // TODO: Maybe Problem
                        auto s = str[i];
                        ++i;
                        if (str[i] == '|' || str[i] == '&')
                        {
                            if (str[i] == s)
                            {
                                tokens.emplace_back(
                                        Token::Operator,
                                        std::string(str.substr(i - 1, 2)));
                                ++i;
                                continue;
                            }
                        }
                        tokens.emplace_back(Token::Operator, s);
                    }
                    else if (IsSymbol(str[i]))
                    {
                        tokens.emplace_back(Token::Symbol, str[i]);
                        ++i;
                    }
                    else if (IsNum(str[i]))
                    {
                        auto first = i;
                        while (IsNum(str[i]) || str[i] == '.')
                        {
                            ++i;
                        }
                        auto s = std::string(str.substr(first, i - first));
                        tokens.emplace_back(Token::NumLiteral, s);
                    }
                }
            }
        }
        return tokens;
    }
}

#endif //INTERPRETER_LEXER_HPP
