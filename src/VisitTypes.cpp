#include "VisitTypes.h"

#include "Entity.h"

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
      return getUniqueID(static_cast<const clang::RecordDecl&>(decl));
    case K::Enum:
      return getUniqueID(static_cast<const clang::EnumDecl&>(decl));
    case K::Function:
      return getUniqueID(static_cast<const clang::FunctionDecl&>(decl));
    case K::Typedef:
      return getUniqueID(static_cast<const clang::TypedefNameDecl&>(decl));
    case K::Var:
      return getUniqueID(static_cast<const clang::VarDecl&>(decl));
    default:
      return reinterpret_cast<uintptr_t>(&decl);
  }
}
}  // namespace

bool ffi::Visitor::checkDecl(const clang::Decl* decl) const {
  if (!decl) return false;
  if (auto ndecl = llvm::dyn_cast<clang::NamedDecl>(decl);
      ndecl && !ndecl->hasExternalFormalLinkage()) {
    auto& diags = context.getDiagnostics();
    if (cfg.WarnNoExternalFormalLinkage) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for entity '%0' is ignored, because it does not "
          "have an external formal linkage.");
      diags.Report(decl->getLocation(), id) << ndecl->getName();
    }
    return false;
  }
  const auto& sm = context.getSourceManager();
  auto ExpansionLoc = sm.getExpansionLoc(decl->getBeginLoc());
  if (ExpansionLoc.isInvalid()) return false;
  if (header_group) return !sm.isInSystemHeader(ExpansionLoc);
  return sm.isInMainFile(ExpansionLoc);
}

bool ffi::Visitor::checkExternC(const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  const bool isDeclExternC = [&decl] {
    if (const auto k = decl.getKind(); k == K::Var)
      return static_cast<const clang::VarDecl&>(decl).isExternC();
    else if (k == K::Function)
      return static_cast<const clang::FunctionDecl&>(decl).isExternC();
    else
      return true;
  }();
  if (!isDeclExternC && !cfg.AssumeExternC && cfg.WarnNoCLinkage) {
    auto& diags = context.getDiagnostics();
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration for entity '%0' is ignored, because it does not "
        "have a C language linkage.");
    auto& ndecl = static_cast<const clang::NamedDecl&>(decl);
    diags.Report(decl.getLocation(), id) << ndecl.getName();
  }
  return isDeclExternC;
}

void ffi::Visitor::matchTranslationUnit(const clang::TranslationUnitDecl& decl,
                                        ModuleContents& mod) const {
  for (auto d : decl.decls()) {
    if (!checkDecl(d)) continue;
    if (auto entity = matchEntity(*d); entity.has_value())
      mod.entities.push_back(std::move(entity.value()));
    else if (auto tag = matchTag(*d); tag.has_value())
      mod.tags.emplace(std::move(tag.value()));
  }
}

std::optional<ffi::Entity> ffi::Visitor::matchEntity(
    const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  switch (decl.getKind()) {
    case K::Var:
      return matchVar(static_cast<const clang::VarDecl&>(decl));
    case K::Function:
      return matchFunction(static_cast<const clang::FunctionDecl&>(decl));
    default:
      return std::nullopt;
  }
}

std::optional<ffi::TagDecl> ffi::Visitor::matchTag(
    const clang::Decl& decl) const {
  using K = clang::Decl::Kind;
  switch (decl.getKind()) {
    case K::Enum:
      return matchStruct(static_cast<const clang::RecordDecl&>(decl));
    case K::CXXRecord:
      return matchStruct(static_cast<const clang::RecordDecl&>(decl));
    case K::Typedef:
      return matchTypedef(static_cast<const clang::TypedefNameDecl&>(decl));
    default:
      return std::nullopt;
  }
}

std::optional<ffi::Type> ffi::Visitor::matchType(
    const clang::NamedDecl& decl, const clang::Type& type) const {
  auto& sm = context.getSourceManager();
  auto& diags = context.getDiagnostics();
  if (auto builtinType = type.getAs<clang::BuiltinType>()) {
    const auto k = builtinType->getKind();
    auto tk = ScalarType::from_clang(k);
    if (!tk.has_value()) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for entity '%0' is ignored, type '%2' is an "
          "OpenCL type, a platform extension, or a clang extension.");
      diags.Report(decl.getLocation(), id) << decl.getName() << name_of(k);
      return std::nullopt;
    }
    return Type{std::move(tk.value())};
  } else if (auto pointerType = type.getAs<clang::PointerType>()) {
    auto pointee = pointerType->getPointeeType();
    auto tk = matchType(decl, *pointee.getTypePtr());
    if (!tk.has_value()) return std::nullopt;
    return Type{PointerType{std::make_unique<Type>(std::move(tk.value()))}};
  } else if (auto refType = type.getAs<clang::ReferenceType>()) {
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration for entity '%0' involving C++ reference is ignored, "
        "because C++ references (lvalue, rvalue) are not supported.");
    diags.Report(decl.getLocation(), id) << decl.getName();
    return std::nullopt;
  } else if (auto typedefType = type.getAs<clang::TypedefType>()) {
    auto typeDecl = typedefType->getDecl();
    auto qualType = typeDecl->getUnderlyingType();
    auto tk = matchType(*typeDecl, *qualType.getTypePtr());
    if (!tk.has_value()) {
      const auto id =
          diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                "in typedef declaration for type alias '%0'.");
      diags.Report(typeDecl->getLocation(), id) << typeDecl->getName();
      return std::nullopt;
    }
    auto& resVal = tk.value().value;
    if (std::holds_alternative<ScalarType>(resVal)) {
      auto name = typeDecl->getName().str();
      if (sm.isInSystemHeader(typeDecl->getLocation()) ||
          cfg.AllowCustomFixedSizeInt)
        if (auto tn = ScalarType::from_name(name); tn.has_value())
          tk = {tn.value()};
    } else if (std::holds_alternative<OpaqueType>(resVal)) {
      auto name = typeDecl->getQualifiedNameAsString();
      auto& opaque = std::get<OpaqueType>(resVal);
      if (opaque.name.empty()) opaque.name = name;
    }
    return tk;
  } else if (auto templType = type.getAs<clang::TemplateSpecializationType>()) {
    auto templName = templType->getTemplateName();
    std::string typeName;
    llvm::raw_string_ostream os{typeName};
    templName.print(os, context.getPrintingPolicy());
    return Type{OpaqueType{typeName}};
  } else if (auto tagType = type.getAs<clang::TagType>()) {
    auto tagDecl = tagType->getDecl();
    auto tagName = tagDecl->getName();
    return Type{OpaqueType{tagName}};
  } else if (auto funcType = type.getAs<clang::FunctionProtoType>()) {
    if (funcType->getCallConv() != clang::CC_C) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Warning,
          "declaration for function pointer '%0' is ignored, because it "
          "does not have a C calling convention.");
      diags.Report(decl.getLocation(), id) << decl.getName();
      return std::nullopt;
    }

    FunctionType func;

    auto retType = matchType(decl, *funcType->getReturnType().getTypePtr());
    if (!retType.has_value()) {
      const auto id = diags.getCustomDiagID(
          clang::DiagnosticsEngine::Note,
          "in declaration for return type of function '%0'.");
      diags.Report(decl.getLocation(), id) << decl.getName();
      return std::nullopt;
    }
    func.returnType = std::make_unique<Type>(std::move(retType.value()));

    auto paramCount = funcType->getNumParams();
    for (auto i = 0; i < paramCount; ++i) {
      auto param = funcType->getParamType(i);
      auto paramType = matchType(decl, *param.getTypePtr());
      if (!paramType.has_value()) {
        const auto id = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Note,
            "in declaration for parameter type %1 of function '%0'.");
        diags.Report(decl.getLocation(), id) << decl.getName() << i;
        return std::nullopt;
      }
      func.params.push_back({"", std::move(paramType.value())});
    }

    return Type{std::move(func)};
  } else {
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "declaration for entity '%0' is ignored for no good reason, please "
        "consider this as a bug, and report to the author.");
    diags.Report(decl.getLocation(), id) << decl.getName();
    return std::nullopt;
  }
}

std::optional<ffi::Entity> ffi::Visitor::matchVarRaw(
    const clang::VarDecl& decl) const {
  auto type = matchType(decl, *decl.getType().getTypePtr());
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
  return Entity{decl.getName(), std::move(type.value())};
}

std::optional<ffi::Entity> ffi::Visitor::matchVar(
    const clang::VarDecl& decl) const {
  auto res = matchVarRaw(decl);
  if (!res.has_value()) {
    auto& diags = context.getDiagnostics();
    const auto id = diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                          "in declaration for variable '%0'.");
    diags.Report(decl.getLocation(), id) << decl.getName();
  }
  return res;
}

std::optional<ffi::Entity> ffi::Visitor::matchFunction(
    const clang::FunctionDecl& decl) const {
  auto& diags = context.getDiagnostics();
  auto name = decl.getName();
  const auto reportNote = [&diags, &decl, &name] {
    const auto noteId = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Note,
        "declaration for function '%0' is therefore ignored.");
    diags.Report(decl.getLocation(), noteId) << name;
    return std::nullopt;
  };

  FunctionType func;
  auto retType = matchType(decl, *decl.getReturnType().getTypePtr());
  if (!retType.has_value()) return reportNote();
  func.returnType = std::make_unique<Type>(std::move(retType.value()));
  for (auto param : decl.parameters()) {
    auto var = matchParam(*param);
    if (!var.has_value()) return reportNote();
    func.params.push_back(std::move(var.value()));
  }

  return Entity{std::move(name), std::move(func)};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchEnum(
    const clang::EnumDecl& decl, llvm::StringRef defName) const {
  auto& diags = context.getDiagnostics();

  auto name = decl.getName();
  if (name.empty()) {
    // Should report warning here
    // but false positive cannot be avoided
    if (defName.empty()) {
      // const auto id = diags.getCustomDiagID(
      //     clang::DiagnosticsEngine::Warning,
      //     "anonymous enum without a typedef'd name is ignored.");
      // diags.Report(decl.getLocation(), id) << name;
      return std::nullopt;
    }
    name = defName;
  }
  auto type = matchType(decl, *decl.getIntegerType().getTypePtr());
  if (!type.has_value()) {
    const auto id =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                              "in the underlying type for enumeration '%0'.");
    diags.Report(decl.getLocation(), id) << name;
    return std::nullopt;
  }

  Enumeration enm;
  enm.underlying_type = std::move(type.value());
  for (auto item : decl.enumerators()) {
    auto itemName = item->getName();
    auto initVal = item->getInitVal().getExtValue();
    enm.values.emplace_back(std::move(itemName), initVal);
  }

  return TagDecl{name, Tag{std::move(enm)}};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchStruct(
    const clang::RecordDecl& decl, llvm::StringRef defName) const {
  auto name = decl.getName();
  if (name.empty()) {
    // Should report warning here
    // but false positive cannot be avoided
    if (defName.empty()) {
      // const auto id = diags.getCustomDiagID(
      //     clang::DiagnosticsEngine::Warning,
      //     "anonymous struct without a typedef'd name is ignored.");
      // diags.Report(decl.getLocation(), id) << name;
      return std::nullopt;
    }
    name = defName;
  }

  return TagDecl{name, Tag{Structure{}}};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchTypedef(
    const clang::TypedefNameDecl& decl) const {
  auto& type = *decl.getUnderlyingType().getTypePtr();
  auto name = decl.getName();
  if (auto enumType = type.getAs<clang::EnumType>())
    return matchEnum(*enumType->getDecl(), name);
  else if (auto structType = type.getAs<clang::RecordType>())
    return matchStruct(*structType->getDecl(), name);
  return std::nullopt;
}

std::optional<ffi::Entity> ffi::Visitor::matchParam(
    const clang::ParmVarDecl& param) const {
  auto& diags = context.getDiagnostics();
  auto res = matchVarRaw(param);
  if (!res.has_value()) {
    const auto id = diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                          "in declaration for parameter '%0'.");
    diags.Report(param.getLocation(), id)
        << param.getName() << param.getSourceRange();
  }
  return res;
}
