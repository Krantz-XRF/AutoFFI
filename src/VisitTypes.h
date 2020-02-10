#pragma once

#include <optional>

#include <clang/AST/AST.h>

#include "Config.h"
#include "Tag.h"
#include "Types.h"

namespace ffi
{
class Visitor
{
  public:
    Visitor(config::Config& cfg, clang::ASTContext& context)
        : cfg{cfg}, context{context}
    {
    }

    std::optional<Type> matchType(const clang::NamedDecl& decl,
                                  const clang::Type& type) const noexcept;
    std::optional<Entity> matchVarRaw(const clang::VarDecl& decl) const
        noexcept;
    std::optional<Entity> matchVar(const clang::VarDecl& decl) const noexcept;
    std::optional<Entity> matchParam(const clang::ParmVarDecl& param) const
        noexcept;
    std::optional<Entity> matchFunction(const clang::FunctionDecl& decl) const
        noexcept;
    std::optional<TagDecl> matchEnum(const clang::EnumDecl& decl,
                                     llvm::StringRef defName = {}) const
        noexcept;
    std::optional<TagDecl> matchStruct(const clang::RecordDecl& decl,
                                       llvm::StringRef defName = {}) const
        noexcept;
    std::optional<TagDecl> matchTypedef(
        const clang::TypedefNameDecl& decl) const noexcept;

  private:
    config::Config& cfg;
    clang::ASTContext& context;
};
} // namespace ffi
