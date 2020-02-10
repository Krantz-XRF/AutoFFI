#include "VisitTypes.h"

#include "Entity.h"

namespace
{
#define TYPE(TypeName)                                                         \
    std::optional<ffi::Type> cast(std::optional<ffi::TypeName>&& x)            \
    {                                                                          \
        if (x.has_value())                                                     \
            return {{std::move(x.value())}};                                   \
        return std::nullopt;                                                   \
    }
#include "Types.def"
} // namespace

std::optional<ffi::Type> ffi::Visitor::matchType(const clang::NamedDecl& decl,
                                                 const clang::Type& type) const
    noexcept
{
    auto& sm = context.getSourceManager();
    auto& diags = context.getDiagnostics();
    if (auto builtinType = type.getAs<clang::BuiltinType>())
    {
        const auto k = builtinType->getKind();
        auto tk = ScalarType::from_clang(k);
        if (!tk.has_value())
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Warning,
                "declaration for entity '%0' is ignored, type '%2' is an "
                "OpenCL type, a platform extension, or a clang extension.");
            diags.Report(decl.getLocation(), id)
                << decl.getName() << name_of(k);
        }
        return cast(std::move(tk));
    }
    else if (auto pointerType = type.getAs<clang::PointerType>())
    {
        auto pointee = pointerType->getPointeeType();
        auto tk = matchType(decl, *pointee.getTypePtr());
        if (!tk.has_value())
            return std::nullopt;
        return {{PointerType{std::make_unique<Type>(std::move(tk.value()))}}};
    }
    else if (auto refType = type.getAs<clang::ReferenceType>())
    {
        const auto id = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Warning,
            "declaration for entity '%0' involving C++ reference is ignored, "
            "because C++ references (lvalue, rvalue) are not supported.");
        diags.Report(decl.getLocation(), id) << decl.getName();
        return std::nullopt;
    }
    else if (auto typedefType = type.getAs<clang::TypedefType>())
    {
        auto typeDecl = typedefType->getDecl();
        auto qualType = typeDecl->getUnderlyingType();
        auto tk = matchType(*typeDecl, *qualType.getTypePtr());
        if (!tk.has_value())
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Note,
                "in typedef declaration for type alias '%0':");
            diags.Report(typeDecl->getLocation(), id) << typeDecl->getName();
            return std::nullopt;
        }
        auto& resVal = tk.value().value;
        if (std::holds_alternative<ScalarType>(resVal))
        {
            auto name = typeDecl->getName().str();
            if (sm.isInSystemHeader(typeDecl->getLocation()) ||
                cfg.AllowCustomFixedSizeInt)
                if (auto tn = ScalarType::from_name(name); tn.has_value())
                    tk = {tn.value()};
        }
        else if (std::holds_alternative<OpaqueType>(resVal))
        {
            auto name = typeDecl->getQualifiedNameAsString();
            auto& opaque = std::get<OpaqueType>(resVal);
            if (opaque.name.empty())
                opaque.name = name;
        }
        return tk;
    }
    else if (auto templType = type.getAs<clang::TemplateSpecializationType>())
    {
        auto templName = templType->getTemplateName();
        std::string typeName;
        llvm::raw_string_ostream os{typeName};
        templName.print(os, context.getPrintingPolicy());
        return {{OpaqueType{typeName}}};
    }
    else if (auto tagType = type.getAs<clang::TagType>())
    {
        auto tagDecl = tagType->getDecl();
        auto tagName = tagDecl->getName();
        return {{OpaqueType{tagName}}};
    }
    else if (auto funcType = type.getAs<clang::FunctionProtoType>())
    {
        if (funcType->getCallConv() != clang::CC_C)
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Warning,
                "declaration for function pointer '%0' is ignored, because it "
                "does not have a C calling convention.");
            diags.Report(decl.getLocation(), id) << decl.getName();
            return std::nullopt;
        }

        FunctionType func;

        auto retType = matchType(decl, *funcType->getReturnType().getTypePtr());
        if (!retType.has_value())
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Note,
                "in declaration for return type of function '%0':");
            diags.Report(decl.getLocation(), id) << decl.getName();
            return std::nullopt;
        }
        func.returnType = std::make_unique<Type>(std::move(retType.value()));

        auto paramCount = funcType->getNumParams();
        for (auto i = 0; i < paramCount; ++i)
        {
            auto param = funcType->getParamType(i);
            auto paramType = matchType(decl, *param.getTypePtr());
            if (!paramType.has_value())
            {
                const auto id = diags.getCustomDiagID(
                    clang::DiagnosticsEngine::Note,
                    "in declaration for parameter type %1 of function '%0':");
                diags.Report(decl.getLocation(), id) << decl.getName() << i;
                return std::nullopt;
            }
            func.params.push_back({"", std::move(paramType.value())});
        }

        return {{std::move(func)}};
    }
    else
    {
        const auto id = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Warning,
            "declaration for entity '%0' is ignored for no good reason, please "
            "consider this as a bug, and report to the author.");
        diags.Report(decl.getLocation(), id) << decl.getName();
        return std::nullopt;
    }
}

std::optional<ffi::Entity> ffi::Visitor::matchVarRaw(
    const clang::VarDecl& decl) const noexcept
{
    auto type = matchType(decl, *decl.getType().getTypePtr());
    if (!type.has_value())
        return std::nullopt;
    return {{decl.getName(), std::move(type.value())}};
}

std::optional<ffi::Entity> ffi::Visitor::matchVar(
    const clang::VarDecl& decl) const noexcept
{
    auto& diags = context.getDiagnostics();
    auto name = decl.getName();
    if (!decl.isExternC() && !cfg.AssumeExternC)
    {
        if (cfg.WarnNoCLinkage)
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Warning,
                "declaration for entity '%0' is ignored, because it does not "
                "have a C language linkage.");
            diags.Report(decl.getLocation(), id) << name;
        }
        return std::nullopt;
    }
    auto res = matchVarRaw(decl);
    if (!res.has_value())
    {
        const auto id =
            diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                  "in declaration for variable '%0':");
        diags.Report(decl.getLocation(), id) << name;
    }
    return res;
}

std::optional<ffi::Entity> ffi::Visitor::matchFunction(
    const clang::FunctionDecl& decl) const noexcept
{
    auto& diags = context.getDiagnostics();

    auto name = decl.getName();

    if (!decl.isExternC() && !cfg.AssumeExternC)
    {
        if (cfg.WarnNoCLinkage)
        {
            const auto id = diags.getCustomDiagID(
                clang::DiagnosticsEngine::Warning,
                "declaration for entity '%0' is ignored, because it does not "
                "have a C language linkage.");
            diags.Report(decl.getLocation(), id) << name;
        }
        return std::nullopt;
    }

    const auto reportNote = [&diags, &decl, &name] {
        const auto noteId = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Note,
            "declaration for function '%0' is therefore ignored.");
        diags.Report(decl.getLocation(), noteId) << name;
        return std::nullopt;
    };

    FunctionType func;
    auto retType = matchType(decl, *decl.getReturnType().getTypePtr());
    if (!retType.has_value())
        return reportNote();
    func.returnType = std::make_unique<Type>(std::move(retType.value()));
    for (auto param : decl.parameters())
    {
        auto var = matchParam(*param);
        if (!var.has_value())
            return reportNote();
        func.params.push_back(std::move(var.value()));
    }

    return {{std::move(name), std::move(func)}};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchEnum(
    const clang::EnumDecl& decl, llvm::StringRef defName) const noexcept
{
    auto& diags = context.getDiagnostics();

    auto name = decl.getName();
    if (name.empty())
    {
        // Should report warning here
        // but false positive cannot be avoided
        if (defName.empty())
        {
            // const auto id = diags.getCustomDiagID(
            //     clang::DiagnosticsEngine::Warning,
            //     "anonymous enum without a typedef'd name is ignored.");
            // diags.Report(decl.getLocation(), id) << name;
            return std::nullopt;
        }
        name = defName;
    }
    auto type = matchType(decl, *decl.getIntegerType().getTypePtr());
    if (!type.has_value())
    {
        const auto id = diags.getCustomDiagID(
            clang::DiagnosticsEngine::Note,
            "in the underlying type for enumeration '%0'.");
        diags.Report(decl.getLocation(), id) << name;
        return std::nullopt;
    }

    Enumeration enm;
    enm.underlying_type = std::move(type.value());
    for (auto item : decl.enumerators())
    {
        auto itemName = item->getName();
        auto initVal = item->getInitVal().getExtValue();
        enm.values.emplace_back(std::move(itemName), initVal);
    }

    return {{name, std::move(enm)}};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchStruct(
    const clang::RecordDecl& decl, llvm::StringRef defName) const noexcept
{
    auto name = decl.getName();
    if (name.empty())
    {
        // Should report warning here
        // but false positive cannot be avoided
        if (defName.empty())
        {
            // const auto id = diags.getCustomDiagID(
            //     clang::DiagnosticsEngine::Warning,
            //     "anonymous struct without a typedef'd name is ignored.");
            // diags.Report(decl.getLocation(), id) << name;
            return std::nullopt;
        }
        name = defName;
    }

    return {{name, Structure{}}};
}

std::optional<ffi::TagDecl> ffi::Visitor::matchTypedef(
    const clang::TypedefNameDecl& decl) const noexcept
{
    auto& type = *decl.getUnderlyingType().getTypePtr();
    auto name = decl.getName();
    if (auto enumType = type.getAs<clang::EnumType>())
        return matchEnum(*enumType->getDecl(), name);
    else if (auto structType = type.getAs<clang::RecordType>())
        return matchStruct(*structType->getDecl(), name);
    return std::nullopt;
}

std::optional<ffi::Entity> ffi::Visitor::matchParam(
    const clang::ParmVarDecl& param) const noexcept
{
    auto& diags = context.getDiagnostics();
    auto res = matchVarRaw(param);
    if (!res.has_value())
    {
        const auto id =
            diags.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                  "in declaration for parameter '%0':");
        diags.Report(param.getLocation(), id)
            << param.getName() << param.getSourceRange();
    }
    return res;
}
