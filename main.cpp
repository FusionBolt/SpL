#include <iostream>
#include <fstream>
#include "Parse.hpp"

std::string ReadFile(const std::string &fileName)
{
    std::ifstream file(fileName, std::ios::in);
    if (!file.is_open())
    {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
}

void LLVMTargetInit()
{
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
}
void OutPutObj(const std::string& objName = "output.o")
{
    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

// Print an error and exit if we couldn't find the requested target.
// This generally occurs if we've forgotten to initialise the
// TargetRegistry or we have a bogus target triple.
    if (!Target) {
        llvm::errs() << Error;
        return;
    }

    auto CPU = "generic";
    auto Features = "";

    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
    In::TheModule->setDataLayout(TargetMachine->createDataLayout());
    In::TheModule->setTargetTriple(TargetTriple);

    auto Filename = objName;
    std::error_code EC;
    llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return;
    }
    llvm::legacy::PassManager pass;
    auto FileType = llvm::CGFT_ObjectFile;

    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        llvm::errs() << "TargetMachine can't emit a file of this type";
        return;
    }

    pass.run(*In::TheModule);
    dest.flush();
}
int main()
{
    In::TheModule = std::make_unique<llvm::Module>("my cool jit", In::TheContext);
    std::string s = ReadFile("../source.sp");

    auto tokens = In::Tokenize(s);
    for (auto& token : tokens)
    {
        token.Output();
    }

     In::Parse p(tokens);
     p.ParseProgram();
     In::TheModule->print(llvm::errs(), nullptr);

     LLVMTargetInit();
     OutPutObj();
}
