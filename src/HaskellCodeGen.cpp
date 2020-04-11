#include "HaskellCodeGen.h"

#include <fmt/format.h>

void ffi::HaskellCodeGen::genModule(const std::string& name,
                                    const ModuleContents& mod) noexcept {
  if (auto p = cfg.FileMarshallers.find(name); p != cfg.FileMarshallers.cend())
    cfg.Marshaller.forward_marshaller = &p->second;
  else
    cfg.Marshaller.forward_marshaller = nullptr;

  namespace fs = llvm::sys::fs;
  auto mname = cfg.ModuleMarshaller.transform(name);
  std::string parentDir = format(FMT_STRING("{}/{}/LowLevel"),
                                 cfg.OutputDirectory, cfg.LibraryName);
  std::string modFile = format(FMT_STRING("{}/{}.hs"), parentDir, mname);
  llvm::sys::fs::create_directories(parentDir);
  std::error_code ec;
  llvm::raw_fd_ostream ofs{modFile, ec};
  if (ec) {
    llvm::errs() << format(FMT_STRING("Cannot open file '{}'.\n"), modFile);
    return;
  }
  os = &ofs;

  *os << "{-# LANGUAGE EmptyDataDecls #-}\n"
         "{-# LANGUAGE ForeignFunctionInterface #-}\n"
         "{-# LANGUAGE PatternSynonyms #-}\n"
         "{-# OPTIONS_GHC -Wno-missing-pattern-synonym-signatures #-}\n"
         "{-# OPTIONS_GHC -Wno-unused-imports #-}\n";
  *os << "module ";
  genModulePrefix();
  genModuleName(name);
  *os << " where\n\n";

  *os << "import Foreign.C.Types\n"
         "import Foreign.C.String\n"
         "import Foreign.Storable\n"
         "import Foreign.Ptr\n"
         "import Foreign.Marshal.Alloc\n";
  *os << '\n';
  for (auto& imp : mod.imports) *os << "import " << imp << '\n';
  if (!mod.imports.empty()) *os << '\n';
  for (auto& [name, tag] : mod.tags) genTag(name, tag);
  for (const auto& entity : mod.entities) genEntity(entity);
}

void ffi::HaskellCodeGen::genModulePrefix() noexcept {
  *os << cfg.LibraryName << ".LowLevel.";
}

void ffi::HaskellCodeGen::genModuleName(const std::string& name) noexcept {
  *os << cfg.ModuleMarshaller.transform(name);
}

void ffi::HaskellCodeGen::genEntity(ConstEntity& entity) noexcept {
  clear_fresh_variable();
  *os << "foreign import ccall \"" << entity.first << "\" ";
  genFuncName(entity.first);
  *os << " :: ";
  genType(entity.second);
  *os << '\n';
}

void ffi::HaskellCodeGen::genFuncName(const std::string& name) noexcept {
  *os << cfg.VarMarshaller.transform(name);
}

void ffi::HaskellCodeGen::genTypeName(const std::string& name) noexcept {
  *os << cfg.TypeMarshaller.transform(name);
}

void ffi::HaskellCodeGen::genConstName(const std::string& name) noexcept {
  *os << cfg.ConstMarshaller.transform(name);
}

void ffi::HaskellCodeGen::genType(const Type& type, bool paren) noexcept {
  const auto idx = type.value.index();
  paren = paren && requiresParen(idx) && !isCString(type);
  if (paren) *os << "(";
  switch (idx) {
#define TYPE(TypeName)                             \
  case index<Type::type, TypeName>:                \
    gen##TypeName(std::get<TypeName>(type.value)); \
    break;
#include "Types.def"
  }
  if (paren) *os << ")";
}

void ffi::HaskellCodeGen::genFunctionType(const FunctionType& func) noexcept {
  for (const auto& tp : func.params) {
    genType(tp.second);
    *os << " -> ";
  }
  *os << "IO ";
  genType(*func.returnType, true);
}

void ffi::HaskellCodeGen::genScalarType(const ScalarType& scalar) noexcept {
  *os << scalar.as_haskell();
}

void ffi::HaskellCodeGen::genOpaqueType(const OpaqueType& opaque) noexcept {
  genTypeName(opaque.name);
}

void ffi::HaskellCodeGen::genPointerType(const PointerType& pointer) noexcept {
  auto& pointee = *pointer.pointee;
  if (isCChar(pointee)) {
    *os << "CString";
    return;
  }

  if (isFunction(pointee))
    *os << "FunPtr ";
  else
    *os << "Ptr ";
  if (isVoid(pointee))
    *os << next_fresh_variable();
  else
    genType(pointee, true);
}

void ffi::HaskellCodeGen::genTag(const std::string& name,
                                 const Tag& tag) noexcept {
  if (std::holds_alternative<Enumeration>(tag.payload))
    genEnum(name, std::get<Enumeration>(tag.payload));
  else if (std::holds_alternative<Structure>(tag.payload))
    genStruct(name, std::get<Structure>(tag.payload));
}

void ffi::HaskellCodeGen::genEnum(const std::string& name,
                                  const Enumeration& enm) noexcept {
  *os << "newtype ";
  genTypeName(name);
  *os << " = ";
  genConstName(name);
  *os << "{ unwrap";
  genTypeName(name);
  *os << " :: ";
  genType(enm.underlying_type);
  *os << " }";
  *os << '\n';
  for (auto& [item, val] : enm.values) genEnumItem(name, item, val);
  *os << '\n';
}

void ffi::HaskellCodeGen::genEnumItem(const std::string& name,
                                      const std::string& item,
                                      intmax_t val) noexcept {
  *os << "pattern ";
  genConstName(item);
  *os << " = ";
  genConstName(name);
  *os << ' ' << val << '\n';
}

void ffi::HaskellCodeGen::genStruct(const std::string& name,
                                    const Structure& str) noexcept {
  *os << "data ";
  genTypeName(name);
  *os << "\n\n";
}

void ffi::HaskellCodeGen::clear_fresh_variable() noexcept {
  fresh_variable.clear();
}

std::string ffi::HaskellCodeGen::next_fresh_variable() noexcept {
  if (!fresh_variable.empty() && fresh_variable.back() != 'z')
    ++fresh_variable.back();
  else
    fresh_variable.push_back('a');
  return fresh_variable;
}

bool ffi::HaskellCodeGen::requiresParen(int tid) noexcept {
  return tid == index<Type::type, FunctionType> ||
         tid == index<Type::type, PointerType>;
}

bool ffi::HaskellCodeGen::isCChar(const Type& type) noexcept {
  if (std::holds_alternative<ScalarType>(type.value)) {
    auto& scalar = std::get<ScalarType>(type.value);
    return scalar.sign == ScalarType::Unspecified &&
           scalar.qualifier == ScalarType::Char &&
           scalar.width == ScalarType::WidthNone;
  }
  return false;
}

bool ffi::HaskellCodeGen::isCString(const Type& type) noexcept {
  if (std::holds_alternative<PointerType>(type.value)) {
    auto& pointer = std::get<PointerType>(type.value);
    return isCChar(*pointer.pointee);
  }
  return false;
}

bool ffi::HaskellCodeGen::isVoid(const Type& type) noexcept {
  if (std::holds_alternative<ScalarType>(type.value)) {
    auto& scalar = std::get<ScalarType>(type.value);
    return scalar.qualifier == ScalarType::Void;
  }
  return false;
}

bool ffi::HaskellCodeGen::isFunction(const Type& type) noexcept {
  return std::holds_alternative<FunctionType>(type.value);
}
