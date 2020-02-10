#include "PrimTypes.h"

#include <iostream>
#include <regex>
#include <sstream>

#include <llvm/IR/DiagnosticInfo.h>

#include <lava/assert.h>
#include <lava/format.h>

namespace fmt = lava::format;

std::string ffi::ScalarType::as_haskell() const noexcept
{
    constexpr const char* chQual[]{"CU", "CS", "C"};
    if (qualifier == Void)
        return "()";
    else if (qualifier == UniChar || qualifier == Exact || qualifier == Least ||
             qualifier == Fast)
        return std::string(sign ? "Int" : "Word") +
               to_string(std::min(width, Width64));
    else if (qualifier == Char)
        return std::string(chQual[sign]) + to_string(qualifier);
    else
        return std::string(sign ? "C" : "CU") + to_string(qualifier);
}

std::optional<ffi::ScalarType> ffi::ScalarType::from_clang(
    clang::BuiltinType::Kind type) noexcept
{
    using K = clang::BuiltinType::Kind;
    const auto signedness = signedness_of(type);
    switch (type)
    {
    case K::Void:
        return ScalarType{None, Void, WidthNone};
    case K::Bool:
        return ScalarType{Unsigned, Bool, WidthNone};
    case K::Char_U:
    case K::Char_S:
        return ScalarType{Unspecified, Char, WidthNone};
    case K::UChar:
    case K::SChar:
        return ScalarType{signedness, Char, WidthNone};
    case K::WChar_U:
    case K::WChar_S:
        return ScalarType{None, WChar, WidthNone};
    case K::Char8:
        return ScalarType{Unsigned, UniChar, Width8};
    case K::Char16:
        return ScalarType{Unsigned, UniChar, Width16};
    case K::Char32:
        return ScalarType{Unsigned, UniChar, Width32};
    case K::UShort:
    case K::Short:
        return ScalarType{signedness, Short, WidthNone};
    case K::UInt:
    case K::Int:
        return ScalarType{signedness, Int, WidthNone};
    case K::ULong:
    case K::Long:
        return ScalarType{signedness, Long, WidthNone};
    case K::ULongLong:
    case K::LongLong:
        return ScalarType{signedness, LLong, WidthNone};
    case K::Float:
        return ScalarType{Signed, Float, WidthNone};
    case K::Double:
        return ScalarType{Signed, Double, WidthNone};
    default:
        return std::nullopt;
    }
}

std::optional<ffi::ScalarType> ffi::ScalarType::from_name(
    std::string_view type) noexcept
{
    static const std::regex reg{
        "(u)?int(ptr|max|(?:_(least|fast))?([1-9][0-9]*))_t",
        std::regex::ECMAScript | std::regex::optimize};
    std::match_results<std::string_view::const_iterator> match;
    std::regex_match(type.cbegin(), type.cend(), match, reg);

    if (match.empty())
        return std::nullopt;

    ffi::ScalarType res{match[1].matched ? Unsigned : Signed, Exact, WidthNone};
    if (const auto c = *match[2].first; c == 'p')
        res.width = WidthPtr;
    else if (c == 'm')
        res.width = WidthMax;
    else if (c == '_')
    {
        if (const auto q = *match[3].first; q == 'l')
            res.qualifier = Least;
        else
            res.qualifier = Fast;
    }
    if (match[4].matched)
    {
        auto match_width = [&match](auto s) {
            return std::equal(match[4].first, match[4].second, s);
        };
        if (match_width("8"))
            res.width = Width8;
        else if (match_width("16"))
            res.width = Width16;
        else if (match_width("32"))
            res.width = Width32;
        else if (match_width("64"))
            res.width = Width64;
        else
            return std::nullopt;
    }
    return res;
}
