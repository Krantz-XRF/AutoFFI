#include <array>
#include <iostream>
#include <map>

#include <clang/AST/AST.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersMacros.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>

#include <lava/assert.h>
#include <lava/format.h>

#include "Config.h"
#include "Entity.h"
#include "Function.h"
#include "HaskellCodeGen.h"
#include "MatcherWrapper.h"
#include "Module.h"
#include "PrimTypes.h"
#include "Types.h"
#include "VisitTypes.h"
#include "YAML.h"

namespace tool = clang::tooling;
namespace fmt = lava::format;
namespace cl = llvm::cl;

namespace
{
cl::OptionCategory category{"auto-FFI Options"};
cl::opt<bool> Help{"h", cl::desc{"Alias for -help"}, cl::Hidden};
cl::opt<bool> DumpConfig{
    "dump-config", cl::desc{"Dump configuration options to stdout and exit."},
    cl::cat{category}};
cl::opt<bool> Yaml{"yaml", cl::desc{"Dump YAML for entities."},
                   cl::cat{category}};
cl::opt<bool> Verbose{"verbose", cl::desc{"Print verbose output message."},
                      cl::cat{category}};
cl::list<std::string> ConfigFiles{cl::Positional,
                                  cl::desc{"[<file> ...] [-- clang options]"},
                                  cl::cat{category}};
const char version[]{"auto-FFI 2020"};
} // namespace

namespace match
{
using namespace clang::ast_matchers;

auto isPublicAPI =
    allOf(hasExternalFormalLinkage(), unless(isExpansionInSystemHeader()),
          anyOf(hasDeclContext(translationUnitDecl()),
                hasDeclContext(linkageSpecDecl())));

DeclarationMatcher variable = varDecl(isPublicAPI).bind("var");
DeclarationMatcher function = functionDecl(isPublicAPI).bind("func");
DeclarationMatcher enums = enumDecl(isPublicAPI).bind("enum");
DeclarationMatcher structs = recordDecl(isPublicAPI).bind("struct");
DeclarationMatcher typedefs = typedefNameDecl(isPublicAPI).bind("typedef");
DeclarationMatcher entity = anyOf(variable, function, typedefs, enums, structs);

class VariableCallback : public MatchFinder::MatchCallback,
                         public tool::SourceFileCallbacks
{
  public:
    VariableCallback(config::Config& cfg) : cfg{cfg}, currentModule{nullptr}
    {
    }

    bool handleBeginSource(clang::CompilerInstance& CI) override
    {
        const auto& fileName = cfg.FileNames[modules.size()];
        currentModule = &modules[fileName];
        isHeaderGroup =
            std::find(cfg.IsHeaderGroup.cbegin(), cfg.IsHeaderGroup.cend(),
                      fileName) != cfg.IsHeaderGroup.cend();
        return true;
    }

    void handleEndSource() override
    {
    }

    bool checkDecl(clang ::SourceManager& sm, const clang::Decl* decl) const
        noexcept
    {
        if (!decl)
            return false;
        if (isHeaderGroup)
            return true;
        auto ExpansionLoc = sm.getExpansionLoc(decl->getBeginLoc());
        if (ExpansionLoc.isInvalid())
            return false;
        return sm.isInMainFile(ExpansionLoc);
    }

    void run(const MatchFinder::MatchResult& result) override
    {
        auto& sm = *result.SourceManager;
        auto& nodes = result.Nodes;
        auto& diags = result.Context->getDiagnostics();
        ffi::Visitor visitor{cfg, *result.Context};
        if (auto decl = nodes.getNodeAs<clang::VarDecl>("var");
            checkDecl(sm, decl))
        {
            auto var = visitor.matchVar(*decl);
            if (var.has_value())
                currentModule->entities.push_back(std::move(var.value()));
        }
        else if (auto decl = nodes.getNodeAs<clang::FunctionDecl>("func");
                 checkDecl(sm, decl))
        {
            auto func = visitor.matchFunction(*decl);
            if (func.has_value())
                currentModule->entities.push_back(std::move(func.value()));
        }
        else if (auto decl = nodes.getNodeAs<clang::EnumDecl>("enum");
                 checkDecl(sm, decl))
        {
            auto enums = visitor.matchEnum(*decl);
            if (enums.has_value())
                currentModule->tags.insert(std::move(enums.value()));
        }
        else if (auto decl = nodes.getNodeAs<clang::RecordDecl>("struct");
                 checkDecl(sm, decl))
        {
            auto structs = visitor.matchStruct(*decl);
            if (structs.has_value())
                currentModule->tags.insert(std::move(structs.value()));
        }
        else if (auto decl = nodes.getNodeAs<clang::TypedefNameDecl>("typedef");
                 checkDecl(sm, decl))
        {
            auto res = visitor.matchTypedef(*decl);
            if (res.has_value())
                currentModule->tags.insert(std::move(res.value()));
        }
    }

    std::map<std::string, ffi::ModuleContents> modules;

  private:
    config::Config& cfg;
    ffi::ModuleContents* currentModule;
    bool isHeaderGroup{false};
};
} // namespace match

int main(int argc, const char* argv[])
{
    // LLVM initialization
    llvm::InitLLVM X(argc, argv);
    cl::HideUnrelatedOptions(category);

    // auto-FFI options
    cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << version << '\n'; });
    cl::ParseCommandLineOptions(
        argc, argv,
        "A tool to generate Haskell FFI binding for C/C++ API.\n\n"
        "It reads the code from the <file>s and writes the result to the "
        "standard output.\n");

    // Help message
    if (Help)
    {
        cl::PrintHelpMessage();
        return 0;
    }

    // Configurations
    config::Config cfg;
    if (DumpConfig)
    {
        llvm::yaml::Output output{llvm::outs()};
        output << cfg;
        return 0;
    }

    // No input, print help and exit
    if (ConfigFiles.empty())
    {
        cl::PrintHelpMessage();
        return 0;
    }

    // Run on config files
    for (const auto& cfgFile : ConfigFiles)
    {
        auto contents = llvm::MemoryBuffer::getFile(cfgFile);
        if (auto ec = contents.getError())
        {
            llvm::errs() << "Failed to load configuration file \"" << cfgFile
                         << "\": " << ec.message() << '\n';
            continue;
        }
        llvm::yaml::Input input{contents.get()->getBuffer()};
        input >> cfg;
        if (input.error())
            continue;

        // Load CWD
        llvm::SmallVector<char, 0> currentPath;
        llvm::sys::fs::current_path(currentPath);
        if (!cfg.RootDirectory.empty())
            llvm::sys::fs::set_current_path(cfg.RootDirectory);

        // Compiler options
        tool::FixedCompilationDatabase compilations{
            cfg.RootDirectory.empty() ? "." : cfg.RootDirectory,
            cfg.CompilerOptions};

        tool::ClangTool tool{compilations, cfg.FileNames};

        match::MatchFinder finder;
        match::VariableCallback varPrinter{cfg};
        finder.addMatcher(match::entity, &varPrinter);

        auto action = tool::newFrontendActionFactory(&finder, &varPrinter);
        auto status = tool.run(action.get());
        if (status)
            return status;

        if (Yaml)
        {
            llvm::yaml::Output output{llvm::outs()};
            output << varPrinter.modules;
        }

        ffi::HaskellCodeGen codeGen{cfg};
        for (auto& [name, mod] : varPrinter.modules)
            codeGen.genModule(name, mod);

        // Recover CWD
        llvm::sys::fs::set_current_path(currentPath);
    }

    return 0;
}
