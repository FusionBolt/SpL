//
// Created by fusionbolt on 2020/8/12.
//

#ifndef INTERPRETER_PARSE_HPP
#define INTERPRETER_PARSE_HPP

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

#include "Lexer.hpp"
#include "AST.hpp"

namespace In
{
    class SymbolTable
    {
    public:
        enum ValKind
        {
            Static,
            Argument,
            Normal
        };

        enum ValType
        {
            Int,
            Float,
        };

        struct Val
        {
            ValKind kind;
            ValType type;
            std::variant<std::string, int, float> val;
        };

        SymbolTable(std::map<std::string, Val> parent = {}) :
                _symbolTable(std::move(parent))
        {
        }

        bool SetValue(const std::string &name, ValKind kind, ValType type, const std::string &value)
        {
            auto oldVal = _symbolTable.find(name);
            if (oldVal != _symbolTable.end())
            {
                // TODO:find in parent
                if (oldVal->second.kind == kind && oldVal->second.type == type)
                {
                    oldVal->second.val = value;
                    return true;
                }
            }
            return false;
        }

        bool Define(const std::string &name, ValKind kind, ValType type, const std::string &value)
        {
            if (_symbolTable.find(name) == _symbolTable.end())
            {
                _symbolTable[name] = Val{kind, type, value};
                return true;
            }
            return false;
        }

    private:
        std::map<std::string, Val> _symbolTable;
    };

    class Parse
    {
    public:
        Parse(std::vector<Token> tokens)
        {
            _tokens = std::move(tokens);
            _currToken = _tokens.begin();
        }

        void ParseProgram()
        { ParseGlobalDeclaration(); }

        void ParseGlobalDeclaration()
        {
            // 3 is (    int a ( | int a = | int *a
            // if ((_currToken + 3)->GetValue() == "(")
            if (LookN(2)->GetValue() == "(")
            {
                auto function = ParseFunctionDeclaration();
                std::cout << function.ToStr() << std::endl;
                function.codegen();
            }
            // int a = | int * a =
            else if (LookN(2)->GetValue() == "=" || LookN(3)->GetValue() == "=")
            {
                ParseAssign();
                MatchValue(";");
            }
            else
            {
                Boom();
            }

            if (_currToken == _tokens.end())
            {
                return;
            }

            ParseGlobalDeclaration();
        }

        // TODO:declaration * int *a, int b
        Param ParseParameterDeclaration()
        {
            std::vector<std::pair<Type, Identifier>> params;
            do
            {
                auto type = MatchValueConditionRet(IsTypeKeyWord);
                auto identifier = MatchTypeRetValue(Token::Identifier);
                params.emplace_back(Type(type), Identifier(identifier));
            } while (MatchLookValue(","));
            return Param(params);
        }

        std::shared_ptr<Statement> ParseReturn()
        {
            if (MatchLookValue("return"))
            {
                // TODO:syntax check, compare return val with return type
                if (MatchLookValue(";"))
                {
                    // TODO: use nullptr?
                    return std::make_shared<Statement>();
                }
                auto expression = ParseExpression();
                MatchValue(";");
                return std::make_shared<Return>(expression);
            }
            return std::make_shared<Statement>();
        }

        std::shared_ptr<Statement> ParseFunctionBody()
        {
            return ParseStatement();
        }

        Function ParseFunctionDeclaration()
        {
            auto returnType = _currToken->GetValue();
            MatchValueCondition(IsTypeKeyWord);
            auto identifier = MatchTypeRetValue(Token::Identifier);
            MatchValue("(");
            Param param;
            if (_currToken->GetValue() != ")")
            {
                param = ParseParameterDeclaration();
            }
            MatchValue(")");
            MatchValue("{");
            auto body = ParseFunctionBody();
            MatchValue("}");
            // TODO:statement
            return Function(Type(returnType), Identifier(identifier), param, body);
        }

        std::shared_ptr<Assign> ParseAssign()
        {
            // TODO:static
            auto type = MatchTypeRetValue(Token::KeyWord);
            auto identifier = MatchTypeRetValue(Token::Identifier);
            MatchValue("=");
            auto expr = ParseExpression();
            return std::make_shared<Assign>(Identifier(identifier), expr);
        }

        Args ParseCallArgs()
        {
            // TODO: *a
            std::vector<std::shared_ptr<Expression>> args;
            while (true)
            {
                auto arg = ParseExpression();
                args.push_back(arg);
                if (!MatchLookValue(","))
                {
                    MatchValue(")");
                    break;
                }
            }
            return args;
        }

        std::shared_ptr<Expression> ParseTerm()
        {
            // TODO:(1 * 2)
            switch (_currToken->GetType())
            {
                case Token::NumLiteral:
                {
                    auto num = MatchTypeRetValue(Token::NumLiteral);
                    return std::make_shared<NumberLiteral>(num);
                }
                case Token::Char:
                {
                    auto c = MatchTypeRetValue(Token::Char);
                    return std::make_shared<StringLiteral>(c);
                }
                case Token::StringLiteral:
                {
                    auto s = MatchTypeRetValue(Token::StringLiteral);
                    return std::make_shared<StringLiteral>(s);
                }
                case Token::Identifier:
                {
                    auto identifier = MatchTypeRetValue(Token::Identifier);
                    // call
                    if (MatchLookValue("("))
                    {
                        auto args = ParseCallArgs();
                        return std::make_shared<Call>(Identifier(identifier), args);
                    }
                    return std::make_shared<Identifier>(identifier);
                    // else is a var
                }
                case Token::KeyWord:
                {
                    if (_currToken->GetValue() == "true" || _currToken->GetValue() == "false")
                    {
                        return std::make_shared<BoolLiteral>(MatchTypeRetValue(Token::KeyWord));
                    }
                    else
                    {
                        Boom();
                    }
                    // op term
                }
                case Token::Operator:
                {
                    auto c = _currToken->GetValue()[0];
                    if (!IsUnaryOp(c))
                    {
                        Boom();
                    }
                    auto val = ParseTerm();
                    return std::make_shared<UnaryOp>(std::string(1, c), val);
                }
                default:
                    Boom();
            }
        }

        std::shared_ptr<Expression> ParseExpression()
        {
            auto left = ParseTerm();
            // TODO:is operator
            while (_currToken->GetType() == Token::Operator)
            {
                auto op = MatchTypeRetValue(Token::Operator);
                auto right = ParseTerm();

                left = std::make_shared<BinaryOp>(op, left, right);
            }
            return left;
        }

        std::shared_ptr<Statement> ParseStatement()
        {
            // TODO:{  { int a = 0; }  }
            // null statement
            if (MatchLookValue(";"))
            {
                std::make_shared<EmptyStatement>();
            }
            // call function | assign var | set var val

            // TODO: search from symbol table
            // int a = 1;
            auto stmt = std::make_shared<Statement>();
            if (IsTypeKeyWord(_currToken->GetValue()))
            {
                stmt->SetLeft(std::make_shared<Statement>(ParseAssign()));
                MatchValue(";");
            }
            else if (_currToken->GetType() == Token::Identifier &&
                     ((LookN(1)->GetValue() == "(") || (LookN(1)->GetValue() == "=")))
            {
                auto identifier = MatchTypeRetValue(Token::Identifier);
                // fun(args..);
                if (MatchLookValue("("))
                {
                    // a, b, 1
                    auto args = ParseCallArgs();
                    stmt->SetLeft(std::make_shared<Statement>(std::make_shared<Call>(Identifier(identifier), args)));
                }
                    // a = 1;
                else
                {
                    MatchValue("=");
                    auto expr = ParseExpression();
                    stmt->SetLeft(std::make_shared<Statement>(std::make_shared<SetNewVal>(Identifier(identifier), expr)));
                }
                MatchValue(";");
            }
            else
            {
                if (MatchLookValue("if"))
                {
                    stmt->SetLeft(ParseIf());
                }
                else if (MatchLookValue("for"))
                {
                    stmt->SetLeft(ParseFor());
                }
                else if (MatchLookValue("while"))
                {
                    stmt->SetLeft(ParseWhile());
                }
                    // stop recursion
                else if (_currToken->GetValue() == "return")
                {
                    // return ParseReturn();
                    stmt->SetLeft(ParseReturn());
                }
                else if (_currToken->GetValue() == "}")
                {
                    return std::make_shared<EmptyStatement>();
                }
                else
                {
                    // expression;
                    stmt->SetExpr(ParseExpression());
                    MatchValue(";");
                }
            }
            stmt->SetRight(ParseStatement());
            return stmt;
        }

        void ParseDeclaration()
        {
            // TODO: pointer, multi value, for example: int *a, b, c;
            auto type = MatchValueConditionRet(IsTypeKeyWord);
            auto identifier = MatchTypeRetValue(Token::Identifier);
            MatchValue("=");
            ParseExpression();
        }

        std::shared_ptr<If> ParseIf()
        {
            MatchValue("(");
            auto test = ParseExpression();
            MatchValue(")");
            std::shared_ptr<Statement> conseq;
            if (MatchLookValue("{"))
            {
                conseq = ParseStatement();
                MatchValue("}");
            }
            else
            {
                conseq = ParseStatement();
            }
            std::shared_ptr<Statement> alt;
            if (MatchLookValue("else"))
            {
                if (MatchLookValue("{"))
                {
                    alt = ParseStatement();
                    MatchValue("}");
                }
            }
            return std::make_shared<If>(test, conseq, alt);
        }

        std::shared_ptr<While> ParseWhile()
        {
            MatchValue("(");
            ParseExpression();
            MatchValue(")");
            if (MatchLookValue("{"))
            {
                ParseStatement();
                MatchValue("}");
            }
            else
            {
                ParseStatement();
            }
        }

        std::shared_ptr<For> ParseFor()
        {
            MatchValue("(");
            // assign int a = 0;
            auto assign = ParseAssign();
            MatchValue(";");
            auto condition = ParseExpression();
            MatchValue(";");
            auto step = ParseExpression();
            MatchValue(")");
            std::shared_ptr<Statement> body;
            if (MatchLookValue("{"))
            {
                body = ParseStatement();
                MatchValue("}");
            }
            else
            {
                // TODO: parse on line
                body = ParseStatement();
            }
            return std::make_shared<For>(assign, condition, step, body);
        }

        void Next()
        {
            ++_currToken;
        }

        template<typename Condition>
        std::string MatchValueConditionRet(Condition &&condition)
        {
            auto v = _currToken->GetValue();
            MatchValueCondition(condition);
            return v;
        }

        std::string MatchValueRet(const std::string &s)
        {
            auto v = _currToken->GetValue();
            MatchValue(s);
            return v;
        }

        template<typename Condition>
        void MatchValueCondition(Condition &&condition)
        {
            if (!condition(_currToken->GetValue()))
            {
                Boom();
            }
            ++_currToken;
        }

        void MatchValue(const std::string &s)
        {
            MatchValueCondition([&](auto &&v)
                                {
                                    return v == s;
                                });
            //        if(_currToken->GetValue() != s)
            //        {
            //            Boom();
            //        }
            //        ++_currToken;
        }

        std::string MatchTypeRetValue(Token::Type type)
        {
            auto v = _currToken->GetValue();
            MatchType(type);
            return v;
        }

        bool MatchLookValue(const std::string &s)
        {
            if (_currToken->GetValue() == s)
            {
                ++_currToken;
                return true;
            }
            return false;
        }

        void MatchType(Token::Type type)
        {
            if (_currToken->GetType() != type)
            {
                Boom();
            }
            ++_currToken;
        }

        bool MatchLookType(Token::Type type)
        {
            if (_currToken->GetType() == type)
            {
                ++_currToken;
                return true;
            }
            return false;
        }

        std::vector<Token>::iterator LookN(size_t n)
        {
            return _currToken + n;
        }

    private:
        std::vector<Token> _tokens;

        std::vector<Token>::iterator _currToken;

        SymbolTable _symbolTable;
    };
}
#endif // INTERPRETER_PARSE_HPP
