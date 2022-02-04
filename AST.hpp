//
// Created by fusionbolt on 2020/8/13.
//
#ifndef INTERPRETER_AST_HPP
#define INTERPRETER_AST_HPP

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <utility>

namespace In
{
    static inline llvm::LLVMContext TheContext;
    static inline llvm::IRBuilder<> Builder(TheContext);
    static inline std::unique_ptr<llvm::Module> TheModule;
    static inline std::map<std::string, llvm::AllocaInst*> NamedValues;

    static llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *theFunction,
            const std::string& varName)
    {
        llvm::IRBuilder<> tmp(&theFunction->getEntryBlock(),
                theFunction->getEntryBlock().begin());
        return tmp.CreateAlloca(llvm::Type::getFloatTy(TheContext), 0,
                varName.c_str());
    }

    llvm::Value *LogErrorV(const std::string& str)
    {
        std::cout << str << std::endl;
        return nullptr;
    }

    struct Expression
    {
        virtual std::string ToStr()
        { return ""; }

        virtual llvm::Value *codegen()
        {
            std::cout << "expr codegen" << std::endl;
            return nullptr;
        }

        virtual ~Expression() = default;
    };

    auto *t = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(TheContext));

    struct Statement
    {
        Statement()
        {
            std::cout << "Statement" << std::endl;
        }

        Statement(std::shared_ptr<Statement> left,
                  std::shared_ptr<Statement> right) :
                _left(std::move(left)),
                _right(std::move(right))
        {
        }

        Statement(std::shared_ptr<Expression> expr,
                  std::shared_ptr<Statement> right = nullptr) :
                _expr(std::move(expr)),
                _right(std::move(right))
        {
        }

        void SetExpr(std::shared_ptr<Expression> expr)
        {
            _expr = std::move(expr);
        }

        void SetLeft(std::shared_ptr<Statement> left)
        {
            _left = std::move(left);
        }
        void SetRight(std::shared_ptr<Statement> right)
        {
            _right = std::move(right);
        }

        virtual std::string ToStr()
        {
            std::string leftStr, rightStr;
            if (_expr != nullptr)
            {
                leftStr = _expr->ToStr() + ";";
            }
            else
            {
                leftStr = _left->ToStr();
            }
            if (_right != nullptr)
            {
                rightStr = _right->ToStr();
            }
            return leftStr + "\n" + rightStr;
        }

        virtual ~Statement() = default;

        virtual llvm::Value *codegen()
        {
            // TODO:多个语句怎么办
            std::cout << "Statement codegen" << std::endl;
            llvm::Value *v;
            if (_expr != nullptr)
            {
                // return _expr->codegen();
                v = _expr->codegen();
            }
            else
            {
                // return _left->codegen();
                v = _left->codegen();
            }
            if(_right != nullptr)
            {
                // TODO:连续两个return会返回第二个，严重问题
                auto *r = _right->codegen();
                if (r != t)
                {
                    return r;
                }
                return v;
            }
            else
            {
                return v;
            }
        }

        std::shared_ptr<Statement> _left, _right;
        std::shared_ptr<Expression> _expr;
    };

    struct EmptyStatement : public Statement
    {
        EmptyStatement()
        {
            std::cout << "Empty Statement" << std::endl;
        }

        std::string ToStr() override
        {
            return "Empty Statement";
        }

        llvm::Value *codegen() override
        {
            // TODO:error
            return t;
        };
    };

    struct Type
    {
        Type(const std::string &type)
        { _type = type; }

        std::string ToStr()
        { return _type; }

        std::string _type;
    };

// TODO: 去掉重复
    struct BinaryOp : public Expression
    {
        BinaryOp(std::string op, std::shared_ptr<Expression> left,
                 std::shared_ptr<Expression> right) :
                _op(std::move(op)),
                _left(std::move(left)), _right(std::move(right))
        {
        }

        std::string ToStr() override
        {
            return _left->ToStr() + " " + _op + " " + _right->ToStr();
        }

        llvm::Value *codegen() override
        {
            llvm::Value *l = _left->codegen();
            llvm::Value *r = _right->codegen();
            if (!l || !r)
            {
                return nullptr;
            }
            switch (_op[0])
            {
                case '+':
                    return Builder.CreateFAdd(l, r, "addtmp");
                case '-':
                    return Builder.CreateFSub(l, r, "subtmp");
                case '*':
                    return Builder.CreateFMul(l, r, "multmp");
                case '<':
                    l = Builder.CreateFCmpULT(l, r, "cmptmp");
                    return Builder.CreateUIToFP(
                            l, llvm::Type::getDoubleTy(TheContext), "booltmp");
                case '>':
                    l = Builder.CreateFCmpULT(r, l, "cmptmp");
                    return Builder.CreateUIToFP(
                            l, llvm::Type::getDoubleTy(TheContext), "booltmp");
                case '=':
                {
                    // TODO:error
                    // NamedValues[_left->ToStr()] = r;
                    llvm::Value *var = NamedValues[_left->ToStr()];
                    Builder.CreateStore(r, var);
                    return r;
                }
                default:
                    return LogErrorV("invalid binary operator" + _op);
            }
        }

        std::string _op;

        std::shared_ptr<Expression> _left, _right;
    };

    struct UnaryOp : public Expression
    {
        UnaryOp(std::string op, std::shared_ptr<Expression> val) :
                _op(std::move(op)), _val(std::move(val))
        {
        }

        std::string ToStr() override
        { return _op + " " + _val->ToStr(); }

        llvm::Value *codegen() override
        { return nullptr; }

        std::string _op;

        std::shared_ptr<Expression> _val;
    };

    struct Identifier : public Expression
    {
        Identifier(std::string name) : _name(std::move(name))
        {}

        std::string Name() const
        { return _name; }

        std::string ToStr() override
        { return _name; }

        llvm::Value *codegen() override
        {
            llvm::Value *v = NamedValues[_name];
            // TODO:remove commet symbol
            if (!v)
            {
                std::string s = "Unknown variable name" + _name;
                return LogErrorV(s.c_str());
            }
            return Builder.CreateLoad(v, _name);
        }

        std::string _name;
    };

// TODO:refactor
    struct NumberLiteral : public Expression
    {
        NumberLiteral(std::string val) : _val(std::move(val))
        {}

        std::string ToStr() override
        { return _val; }

        llvm::Value *codegen() override
        {
            return llvm::ConstantFP::get(TheContext, llvm::APFloat(std::stof(_val)));
        }

        std::string _val;
    };

    struct StringLiteral : public Expression
    {
        StringLiteral(const std::string &val) : _val(val)
        {}

        std::string ToStr() override
        { return _val; }

        llvm::Value *codegen() override
        { return nullptr; }

        std::string _val;
    };

    struct BoolLiteral : public Expression
    {
        BoolLiteral(const std::string &val) : _val(val)
        {}

        std::string ToStr() override
        { return _val; }

        llvm::Value *codegen() override
        { return nullptr; }

        std::string _val;
    };

    struct Args
    {
        Args() = default;

        Args(std::vector<std::shared_ptr<Expression>> exprs) :
                _exprs(std::move(exprs))
        {
        }

        [[nodiscard]] size_t Size() const
        { return _exprs.size(); }

        std::string ToStr()
        {
            std::string str;
            for (auto &&expr : _exprs)
            {
                str += (expr->ToStr() + ",");
            }
            if (!str.empty())
            {
                str.pop_back();
            }
            return str;
        }

        std::vector<std::shared_ptr<Expression>> _exprs;
    };

    struct Param
    {
        Param() = default;

        Param(std::vector<std::pair<Type, Identifier>> params) :
                _params(std::move(params))
        {
        }

        std::vector<std::pair<Type, Identifier>> _params;

        std::string ToStr()
        {
            std::string str;
            for (auto &&param : _params)
            {
                str += param.first.ToStr() + " " + param.second.ToStr() + ",";
            }
            if (!str.empty())
            {
                str.pop_back();
            }
            return str;
        }

        // TODO:重构，改为function proto
        llvm::Function* codegen(const std::string& functionName)
        {
            std::vector<llvm::Type *> doubles(_params.size(),
                                              llvm::Type::getFloatTy(TheContext));
            llvm::FunctionType *ft = llvm::FunctionType::get(
                    llvm::Type::getFloatTy(TheContext), doubles, false);
            llvm::Function *f = llvm::Function::Create(
                    ft, llvm::Function::ExternalLinkage, functionName, TheModule.get());
            if (f == nullptr)
            {
                Boom();
            }
            unsigned index = 0;
            for (auto &arg : f->args())
            {
                std::cout << "param name:" << _params[index].second.Name() << std::endl;
                arg.setName(_params[index++].second.Name());
            }
            return f;
        }
    };

    struct Call : public Expression
    {
        Call(Identifier identifier, Args args = {}) :
                _identifier(std::move(identifier)), _args(std::move(args))
        {
        }

        std::string ToStr() override
        {
            return _identifier.ToStr() + "(" + _args.ToStr() + ")";
        }

        llvm::Value *codegen() override
        {
            llvm::Function *calleeF = TheModule->getFunction(_identifier._name);
            if (!calleeF)
            {
                return LogErrorV("Unknown function referenced");
            }
            if (calleeF->arg_size() != _args.Size())
            {
                return LogErrorV("Incorrect arguments passed");
            }
            std::vector<llvm::Value *> argsV;
            for (int i = 0; i < _args.Size(); ++i)
            {
                argsV.push_back(_args._exprs[i]->codegen());
                if (!argsV.back())
                {
                    return nullptr;
                }
            }
            return Builder.CreateCall(calleeF, argsV, "calltmp");
        }

        Identifier _identifier;

        Args _args;
    };

    struct Assign : public Expression
    {
        Assign(Identifier name, std::shared_ptr<Expression> val):
            _name(std::move(name)), _val(std::move(val))
        {

        }

        std::string ToStr() override
        {
            return _name.ToStr() + " = " + _val->ToStr();
        }

        llvm::Value *codegen() override
        {
            // TODO:assign和binary operator里的=，注意改掉
            // assign检查变量是否存在，如果存在则报重复的错误
            // TODO:maybe problem
            auto *function = Builder.GetInsertBlock()->getParent();
            auto *alloca = CreateEntryBlockAlloca(function, _name.Name());
            auto *val = _val->codegen();
            // NamedValues[_name.Name()] = _val->codegen();
            Builder.CreateStore(val, alloca);
            NamedValues[_name.Name()] = alloca;
            return val;
        }
        Identifier _name;
        std::shared_ptr<Expression> _val;
    };

    struct SetNewVal : public Expression
    {
        SetNewVal(Identifier name, std::shared_ptr<Expression> val):
                _name(std::move(name)), _val(std::move(val))
        {

        }

        std::string ToStr() override
        {
            return _name.ToStr() + " = " + _val->ToStr();
        }

        llvm::Value *codegen() override
        {
            auto *val = _val->codegen();
            auto *alloca = NamedValues[_name.Name()];
            Builder.CreateStore(val, alloca);
            return val;
        }
        Identifier _name;
        std::shared_ptr<Expression> _val;
    };

    struct Return : public Statement
    {
        Return(std::shared_ptr<Expression> expr) : _expr(std::move(expr))
        {}

        std::shared_ptr<Expression> _expr;

        std::string ToStr() override
        { return "return " + _expr->ToStr() + ";"; }

        llvm::Value *codegen() override
        { return _expr->codegen(); }
    };

    struct If : public Statement
    {
        If(std::shared_ptr<Expression> cond, std::shared_ptr<Statement> conseq,
           std::shared_ptr<Statement> alt) :
                _cond(std::move(cond)),
                _conseq(std::move(conseq)), _alt(std::move(alt))
        {
        }

        std::shared_ptr<Expression> _cond;
        std::shared_ptr<Statement> _conseq, _alt;

        std::string ToStr() override
        {
            // TODO:alt
            std::string altStr;
            if (_alt != nullptr)
            {
                altStr = "else\n{\n" + _alt->ToStr() + "\n}\n";
            }
            return "if(" + _cond->ToStr() + ")\n{\n" + _conseq->ToStr() + "\n}\n"
                   + altStr;
        }

        llvm::Value *codegen() override
        {
            llvm::Value *cond = _cond->codegen();
            if(cond == nullptr)
            {
                return nullptr;
            }
            // Convert condition to a bool by comparing non-equal to 0.0.
            cond = Builder.CreateFCmpONE(cond, llvm::ConstantFP::get(TheContext,
                    llvm::APFloat(0.0)), "ifcond");
            // TODO:感觉有问题
            llvm::Function *function = Builder.GetInsertBlock()->getParent();
            // 参数带了function，自动将块插入到function的末尾
            llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(TheContext,
                    "then", function);
            llvm::BasicBlock *elseBlock = llvm::BasicBlock::Create(TheContext, "else");
            // 所有基本块通过控制流终止 branch / return
            llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(TheContext, "ifcont");
            Builder.CreateCondBr(cond, thenBlock, elseBlock);

            // then value
            // 从ThenBlock的位置开始插入IR
            Builder.SetInsertPoint(thenBlock);
            llvm::Value *thenVal = _conseq->codegen();
            if(thenVal == nullptr)
            {
                return thenVal;
            }
            // 创建一个 通过branch到达if后的内容 的IR
            // br label %ifconf
            Builder.CreateBr(mergeBlock);
            // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
            thenBlock = Builder.GetInsertBlock();

            // else block
            // 在后面添加一个块
            function->getBasicBlockList().push_back(elseBlock);
            Builder.SetInsertPoint(elseBlock);

            llvm::Value *elseVal = _alt->codegen();
            if(elseVal == nullptr)
            {
                return nullptr;
            }
            Builder.CreateBr(mergeBlock);
            // codegen of 'Else' can change the current block, update ElseBB for the PHI.
            elseBlock = Builder.GetInsertBlock();

            // merge block
            function->getBasicBlockList().push_back(mergeBlock);
            Builder.SetInsertPoint(mergeBlock);
            llvm::PHINode *pn = Builder.CreatePHI(llvm::Type::getFloatTy(TheContext),
                    2, "iftmp");
            pn->addIncoming(thenVal, thenBlock);
            pn->addIncoming(elseVal, elseBlock);
            return pn;
        }
    };

    struct For : public Statement
    {
        For(std::shared_ptr<Expression> init,
                std::shared_ptr<Expression> condition,
                std::shared_ptr<Expression> step,
                std::shared_ptr<Statement> body):
                _init(std::move(init)), _condition(std::move(condition)),
                _body(std::move(body)), _step(std::move(step))
        {

        }

        std::string ToStr() override
        {
            return "for(" + _init->ToStr() + ";" + _condition->ToStr() +
                ";" + _step->ToStr() + ")\n{\n" + _body->ToStr() + "\n}\n";
        }

        llvm::Value *codegen() override
        {
            auto *theFunction = Builder.GetInsertBlock()->getParent();
            std::string varName = std::dynamic_pointer_cast<Assign>(_init)->_name.ToStr();
            auto *alloca = CreateEntryBlockAlloca(theFunction, varName);
            auto *init = _init->codegen();
            if(init == nullptr)
            {
                return nullptr;
            }
            // TODO:error
            Builder.CreateStore(init, alloca);

            // auto *preHeaderBlock = Builder.GetInsertBlock();
            auto *loopBlock = llvm::BasicBlock::Create(TheContext, "loop", theFunction);

            Builder.CreateBr(loopBlock);

            Builder.SetInsertPoint(loopBlock);
//            auto *variable = Builder.CreatePHI(llvm::Type::getFloatTy(TheContext),
//                    2, varName);
//            variable->addIncoming(init, preHeaderBlock);

            auto *oldVal = NamedValues[varName];
            // NamedValues[varName] = variable;
            NamedValues[varName] = alloca;

            if(_body->codegen() == nullptr)
            {
                return nullptr;
            }

            llvm::Value *stepVal = nullptr;
            if(_step)
            {
                stepVal = _step->codegen();
                if(stepVal == nullptr)
                {
                    return nullptr;
                }
            }
            else
            {
                stepVal = llvm::ConstantFP::get(TheContext, llvm::APFloat(1.0));
            }

            // auto *nextVar = Builder.CreateFAdd(variable, stepVal, "nextvar");

            auto *endCond = _condition->codegen();
            if(endCond == nullptr)
            {
                return nullptr;
            }
            auto *curVar = Builder.CreateLoad(alloca, varName);
            // TODO: step用法不一样
            auto *nextVar = Builder.CreateFAdd(curVar, stepVal, "nextvar");
            Builder.CreateStore(nextVar, alloca);

            endCond = Builder.CreateFCmpONE(endCond,
                    llvm::ConstantFP::get(TheContext, llvm::APFloat(0.0)), "loop cond");

            // auto *loopEndBlock = Builder.GetInsertBlock();
            auto *afterBlock = llvm::BasicBlock::Create(
                    TheContext, "afterLoop", theFunction);
            Builder.CreateCondBr(endCond, loopBlock, afterBlock);
            Builder.SetInsertPoint(afterBlock);

            // variable->addIncoming(nextVar, loopEndBlock);
            if(oldVal)
            {
                NamedValues[varName] = oldVal;
            }
            else
            {
                NamedValues.erase(varName);
            }
            return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(TheContext));
        }

        std::shared_ptr<Expression> _init, _condition, _step;
        std::shared_ptr<Statement> _body;
    };

    struct While : public Statement
    {
    };

    struct Function
    {
        // function name and ret val
        Function(Type type, Identifier name, Param params,
                 std::shared_ptr<Statement> body) :
                _type(std::move(type)),
                _name(std::move(name)), _params(std::move(params)),
                _body(std::move(body))
        {
        }

        std::string ToStr()
        {
            std::string bodyStr;
            if (_body != nullptr)
            {
                bodyStr = _body->ToStr();
            }
            return _type.ToStr() + " " + _name.ToStr() + " (" + _params.ToStr()
                   + ")" + "\n{\n" + bodyStr + "}\n";
        }

        llvm::Function *codegen()
        {
            // TODO:function prototype
            // TODO:type
            // prototype->codegen()

            // params type
            // changed

            auto *f = _params.codegen(_name.ToStr());
            // function->codegen
            llvm::Function *theFunction = TheModule->getFunction(_name.Name());
            if (!theFunction)
            {
                theFunction = f;
            }
            if (!theFunction)
            {
                return nullptr;
            }

            if (!theFunction->empty())
            {
                return (llvm::Function *) LogErrorV("Function can't be redefined");
            }
            // 创建新的BasicBlock
            llvm::BasicBlock *bb =
                    llvm::BasicBlock::Create(TheContext, "entry", theFunction);
            // 之后的指令的插入点设置为BasicBlock后面
            Builder.SetInsertPoint(bb);
            // TODO:可能有问题
            NamedValues.clear();
            for (auto &arg : theFunction->args())
            {
                auto *alloca = CreateEntryBlockAlloca(theFunction, arg.getName());
                Builder.CreateStore(&arg, alloca);
                // NamedValues[arg.getName()] = &arg;
                NamedValues[arg.getName()] = alloca;
            }
            if (llvm::Value *retVal = _body->codegen())
            {
                Builder.CreateRet(retVal);
                llvm::verifyFunction(*theFunction);
                
                return theFunction;
            }
            // Error reading body, remove function.
            theFunction->eraseFromParent();
            return nullptr;
        }

        Type _type;
        Identifier _name;
        Param _params;
        std::shared_ptr<Statement> _body;
    };
}
#endif // INTERPRETER_AST_HPP
