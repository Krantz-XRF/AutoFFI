#include "PrimTypes.h"

#include <iostream>
#include <regex>
#include <sstream>

#include <llvm/IR/DiagnosticInfo.h>

#include <lava/assert.h>

#include <fmt/format.h>

std::string ffi::ScalarType::as_haskell() const noexcept {
  constexpr const char* chQual[]{"CU", "CS", "C"};
  if (qualifier == Void)
    return "()";
  else if (qualifier >= UniChar && qualifier <= Fast)
    return fmt::format("{}{}", sign ? "Int" : "Word", to_string(width));
  else if (qualifier == Char || qualifier == Wchar)
    return fmt ::format("{}{}", chQual[sign], to_string(qualifier));
  else if (qualifier == Special)
    return fmt::format("{}Int{}", sign ? "C" : "CU", to_string(width));
  else if (qualifier == Ptrdiff || qualifier == Size)
    return fmt::format("C{}", to_string(qualifier));
  else
    return fmt::format("{}{}", sign ? "C" : "CU", to_string(qualifier));
}

std::optional<ffi::ScalarType> ffi::ScalarType::from_clang(
    clang::BuiltinType::Kind type) noexcept {
  using K = clang::BuiltinType::Kind;
  const auto signedness = signedness_of(type);
  switch (type) {
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
      return ScalarType{Unspecified, Wchar, WidthNone};
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
    std::string_view type) noexcept {
  static const std::map<std::string_view, ScalarType> scalar_types{
      {"wchar_t", {Unspecified, Wchar, WidthNone}},
      {"char8_t", {Unsigned, UniChar, Width8}},
      {"char16_t", {Unsigned, UniChar, Width16}},
      {"char32_t", {Unsigned, UniChar, Width32}},
      {"size_t", {None, Size, WidthNone}},
      {"ptrdiff_t", {None, Ptrdiff, WidthNone}},
  };
  if (auto p = scalar_types.find(type); p != scalar_types.cend())
    return p->second;

  static const std::regex reg{
      "(u)?int(ptr|max|(?:_(least|fast))?([1-9][0-9]*))_t",
      std::regex::ECMAScript | std::regex::optimize};
  std::match_results<std::string_view::const_iterator> match;
  std::regex_match(type.cbegin(), type.cend(), match, reg);

  if (match.empty()) return std::nullopt;

  ffi::ScalarType res{match[1].matched ? Unsigned : Signed, Special, WidthNone};
  if (const auto c = *match[2].first; c == 'p')
    res.width = WidthPtr;
  else if (c == 'm')
    res.width = WidthMax;
  else if (c == '_')
    res.qualifier = *match[3].first == 'l' ? Least : Fast;
  if (match[4].matched) {
    constexpr std::tuple<std::string_view, Width> width[]{
        {"8", Width8}, {"16", Width16}, {"32", Width32}, {"64", Width64}};
    for (const auto& [w, v] : width)
      if (match[4].str() == w) {
        res.width = v;
        break;
      }
    res.qualifier = Exact;
  }
  return res;
}
