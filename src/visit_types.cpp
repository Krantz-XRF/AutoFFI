/* This file is part of auto-FFI (https://github.com/Krantz-XRF/auto-FFI).
 * Copyright (C) 2020 Xie Ruifeng
 *
 * auto-FFI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * auto-FFI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with auto-FFI.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "visit_types.h"

namespace {
constexpr uintptr_t invalid_id = 0;

template <typename T>
inline uintptr_t getUniqueID(const T& decl) {
  const auto p = decl.getFirstDecl();
  return reinterpret_cast<uintptr_t>(p);
}

inline uintptr_t getUniqueID(const clang::Decl& decl) {
  using K = clang::Decl::Kind;
  switch (decl.getKind()) {
    case K::CXXRecord:
      return getUniqueID(llvm::cast<clang::RecordDecl>(decl));
    case K::Enum:
      return getUniqueID(llvm::cast<clang::EnumDecl>(decl));
    case K::Function:
      return getUniqueID(llvm::cast<clang::FunctionDecl>(decl));
    case K::Typedef:
      return getUniqueID(llvm::cast<clang::TypedefNameDecl>(decl));
    case K::Var:
      return getUniqueID(llvm::cast<clang::VarDecl>(decl));
    default:
      return reinterpret_cast<uintptr_t>(&decl);
  }
}
}  // namespace

bool ffi::ast_visitor::check_decl(const clang::Decl* decl) const {
  if (!decl) return false;
  if (const auto ndecl = llvm::dyn_cast<clang::NamedDecl>(decl);
      ndecl && !ndecl->hasExternalFormalLinkage()) {
    auto& diags = context.getDiagnostics();
    if (cfg.warn_no_external_formal_linkage) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for entity '%0' is ignored, because it does not "
          "have an external formal linkage.");
      diags.Report(decl->getLocation(), id) << ndecl->getName();
    }
    return false;
  }
  const auto& sm = context.getSourceManager();
  const auto expansion_loc = sm.getExpansionLoc(decl->getBeginLoc());
  if (expansion_loc.isInvalid()) return false;
  if (header_group) return !sm.isInSystemHeader(expansion_loc);
  return sm.isInMainFile(expansion_loc);
}

bool ffi::ast_visitor::check_extern_c(const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  const bool isDeclExternC = [&decl] {
    if (const auto k = decl.getKind(); k == K::Var)
      return llvm::cast<clang::VarDecl>(decl).isExternC();
    else if (k == K::Function)
      return llvm::cast<clang::FunctionDecl>(decl).isExternC();
    return true;
  }();
  if (!isDeclExternC && !cfg.assume_extern_c && cfg.warn_no_c_linkage) {
    auto& diags = context.getDiagnostics();
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration for entity '%0' is ignored, because it does not "
        "have a C language linkage.");
    const auto& ndecl = llvm::cast<clang::NamedDecl>(decl);
    diags.Report(decl.getLocation(), id) << ndecl.getName();
  }
  return isDeclExternC;
}

void ffi::ast_visitor::match_translation_unit(
    const clang::TranslationUnitDecl& decl, module_contents& mod) const {
  for (auto d : decl.decls()) {
    if (!check_decl(d)) continue;
    if (auto entity = match_entity(*d); entity.has_value())
      mod.entities.try_emplace(std::move(entity->first),
                               std::move(entity->second));
    else if (auto tag = match_tag(*d); tag.has_value())
      mod.tags.emplace(std::move(tag.value()));
  }
}

std::optional<ffi::entity> ffi::ast_visitor::match_entity(
    const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  switch (decl.getKind()) {
    case K::Var:
      return match_var(static_cast<const clang::VarDecl&>(decl));
    case K::Function:
      return match_function(static_cast<const clang::FunctionDecl&>(decl));
    default:
      return std::nullopt;
  }
}

std::optional<ffi::tag_decl> ffi::ast_visitor::match_tag(
    const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  switch (decl.getKind()) {
    case K::Enum:
      return match_enum(static_cast<const clang::EnumDecl&>(decl));
    case K::Record:
    case K::CXXRecord:
      return match_struct(static_cast<const clang::RecordDecl&>(decl));
    case K::Typedef:
      return match_typedef(static_cast<const clang::TypedefNameDecl&>(decl));
    default:
      return std::nullopt;
  }
}

std::optional<ffi::ctype> ffi::ast_visitor::match_type(
    const clang::NamedDecl& decl, const clang::Type& type) const {
  auto& sm = context.getSourceManager();
  auto& diags = context.getDiagnostics();
  if (auto typedefType = type.getAs<clang::TypedefType>()) {
    auto typeDecl = typedefType->getDecl();
    auto qualType = typeDecl->getUnderlyingType();
    auto tk = match_type(*typeDecl, *qualType.getTypePtr());
    if (!tk.has_value()) {
      const auto id =
          diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                "in typedef declaration for type alias '%0'.");
      diags.Report(typeDecl->getLocation(), id) << typeDecl->getName();
      return std::nullopt;
    }
    auto& resVal = tk.value().value;
    if (std::holds_alternative<scalar_type>(resVal)) {
      auto name = typeDecl->getName().str();
      if (sm.isInSystemHeader(typeDecl->getLocation()) ||
          cfg.allow_custom_fixed_size_int)
        if (auto tn = scalar_type::from_name(name); tn.has_value())
          tk = {tn.value()};
    } else if (std::holds_alternative<opaque_type>(resVal)) {
      auto name = typeDecl->getQualifiedNameAsString();
      auto& opaque = std::get<opaque_type>(resVal);
      if (opaque.name.empty()) opaque.name = name;
    }
    return tk;
  }
  if (auto builtinType = type.getAs<clang::BuiltinType>()) {
    const auto k = builtinType->getKind();
    auto tk = scalar_type::from_clang(k);
    if (!tk.has_value()) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for entity '%0' is ignored, type '%2' is an "
          "OpenCL type, a platform extension, or a clang extension.");
      diags.Report(decl.getLocation(), id) << decl.getName() << name_of(k);
      return std::nullopt;
    }
    return ctype{std::move(tk.value())};
  }
  if (auto pointerType = type.getAs<clang::PointerType>()) {
    auto pointee = pointerType->getPointeeType();
    auto tk = match_type(decl, *pointee.getTypePtr());
    if (!tk.has_value()) return std::nullopt;
    return ctype{pointer_type{std::make_unique<ctype>(std::move(tk.value()))}};
  }
  if (type.getAs<clang::ReferenceType>()) {
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration for entity '%0' involving C++ reference is ignored, "
        "because C++ references (lvalue, rvalue) are not supported.");
    diags.Report(decl.getLocation(), id) << decl.getName();
    return std::nullopt;
  }
  if (auto templType = type.getAs<clang::TemplateSpecializationType>()) {
    auto templName = templType->getTemplateName();
    std::string typeName;
    llvm::raw_string_ostream os{typeName};
    templName.print(os, context.getPrintingPolicy());
    return ctype{opaque_type{typeName}};
  }
  if (auto tagType = type.getAs<clang::TagType>()) {
    auto tagDecl = tagType->getDecl();
    auto tagName = tagDecl->getName();
    return ctype{opaque_type{tagName}};
  }
  if (auto funcType = type.getAs<clang::FunctionProtoType>()) {
    if (funcType->getCallConv() != clang::CC_C) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for match_function pointer '%0' is ignored, because it "
          "does not have a C calling convention.");
      diags.Report(decl.getLocation(), id) << decl.getName();
      return std::nullopt;
    }

    function_type func{};
    auto retType = match_type(decl, *funcType->getReturnType().getTypePtr());
    if (!retType.has_value()) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Note,
          "in declaration for return type of match_function '%0'.");
      diags.Report(decl.getLocation(), id) << decl.getName();
      return std::nullopt;
    }
    func.return_type = std::make_unique<ctype>(std::move(retType.value()));

    auto paramCount = funcType->getNumParams();
    for (auto i = 0; i < paramCount; ++i) {
      auto param = funcType->getParamType(i);
      auto paramType = match_type(decl, *param.getTypePtr());
      if (!paramType.has_value()) {
        const auto id = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Note,
            "in declaration for parameter type %1 of match_function '%0'.");
        diags.Report(decl.getLocation(), id) << decl.getName() << i;
        return std::nullopt;
      }
      func.params.push_back({"", std::move(paramType.value())});
    }

    return ctype{std::move(func)};
  }

  const auto id = diags.getCustomDiagID(
      clang::DiagnosticsEngine::Warning,
      "declaration for entity '%0' is ignored for no good reason, please "
      "consider this as a bug, and report to the author.");
  diags.Report(decl.getLocation(), id) << decl.getName();
  return std::nullopt;
}

std::optional<ffi::entity> ffi::ast_visitor::match_var_raw(
    const clang::VarDecl& decl) const {
  auto type = match_type(decl, *decl.getType().getTypePtr());
  if (!type.has_value()) return std::nullopt;
  if (!is_marshallable(type.value())) {
    auto& diags = context.getDiagnostics();
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration is ignored, because its type is not marshallable "
        "(marshallable types: integers, floating points, and pointers).");
    diags.Report(decl.getLocation(), id);
    return std::nullopt;
  }
  return entity{decl.getName(), std::move(type.value())};
}

std::optional<ffi::entity> ffi::ast_visitor::match_var(
    const clang::VarDecl& decl) const {
  auto res = match_var_raw(decl);
  if (!res.has_value()) {
    auto& diags = context.getDiagnostics();
    const auto id = diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                          "in declaration for variable '%0'.");
    diags.Report(decl.getLocation(), id) << decl.getName();
  }
  return res;
}

std::optional<ffi::entity> ffi::ast_visitor::match_function(
    const clang::FunctionDecl& decl) const {
  auto& diags = context.getDiagnostics();
  auto name = decl.getName();
  const auto reportNote = [&diags, &decl, &name] {
    const auto noteId = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Note,
        "declaration for match_function '%0' is therefore ignored.");
    diags.Report(decl.getLocation(), noteId) << name;
    return std::nullopt;
  };

  function_type func{};
  auto retType = match_type(decl, *decl.getReturnType().getTypePtr());
  if (!retType.has_value()) return reportNote();
  func.return_type = std::make_unique<ctype>(std::move(retType.value()));
  for (auto param : decl.parameters()) {
    auto var = match_param(*param);
    if (!var.has_value()) return reportNote();
    func.params.push_back(std::move(var.value()));
  }

  return entity{std::move(name), ctype{std::move(func)}};
}

std::optional<ffi::tag_decl> ffi::ast_visitor::match_enum(
    const clang::EnumDecl& decl, llvm::StringRef def_name) const {
  auto& diags = context.getDiagnostics();

  auto name = decl.getName();
  if (name.empty()) {
    // Should report warning here
    // but false positive cannot be avoided
    if (def_name.empty()) {
      // const auto id = diags.getCustomDiagID(
      //     clang::DiagnosticsEngine::Warning,
      //     "anonymous enum without a typedef'd name is ignored.");
      // diags.Report(decl.getLocation(), id) << name;
      return std::nullopt;
    }
    name = def_name;
  }
  auto type = match_type(decl, *decl.getIntegerType().getTypePtr());
  if (!type.has_value()) {
    const auto id =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                              "in the underlying type for enumeration '%0'.");
    diags.Report(decl.getLocation(), id) << name;
    return std::nullopt;
  }

  enumeration enm;
  enm.underlying_type = std::move(type.value());
  for (auto item : decl.enumerators()) {
    auto itemName = item->getName();
    auto initVal = item->getInitVal().getExtValue();
    enm.values.try_emplace(std::move(itemName), initVal);
  }

  return tag_decl{name, tag_type{std::move(enm)}};
}

std::optional<ffi::tag_decl> ffi::ast_visitor::match_struct(
    const clang::RecordDecl& decl, llvm::StringRef def_name) const {
  auto name = decl.getName();
  if (name.empty()) {
    // Should report warning here
    // but false positive cannot be avoided
    if (def_name.empty()) {
      // const auto id = diags.getCustomDiagID(
      //     clang::DiagnosticsEngine::Warning,
      //     "anonymous struct without a typedef'd name is ignored.");
      // diags.Report(decl.getLocation(), id) << name;
      return std::nullopt;
    }
    name = def_name;
  }

  structure record;
  for (auto f : decl.fields()) {
    auto type = match_type(*f, *f->getType().getTypePtr());
    if (!type.has_value()) return std::nullopt;
    record.fields.push_back({f->getName(), std::move(type.value())});
  }

  return tag_decl{name, tag_type{std::move(record)}};
}

std::optional<ffi::tag_decl> ffi::ast_visitor::match_typedef(
    const clang::TypedefNameDecl& decl) const {
  const auto& type = *decl.getUnderlyingType().getTypePtr();
  const auto name = decl.getName();
  if (const auto enum_type = type.getAs<clang::EnumType>())
    return match_enum(*enum_type->getDecl(), name);
  if (const auto struct_type = type.getAs<clang::RecordType>())
    return match_struct(*struct_type->getDecl(), name);
  return std::nullopt;
}

std::optional<ffi::entity> ffi::ast_visitor::match_param(
    const clang::ParmVarDecl& param) const {
  auto& diags = context.getDiagnostics();
  auto res = match_var_raw(param);
  if (!res.has_value()) {
    const auto id = diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                          "in declaration for parameter '%0'.");
    diags.Report(param.getLocation(), id)
        << param.getName() << param.getSourceRange();
  }
  return res;
}
