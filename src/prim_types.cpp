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

#include "prim_types.h"

#include <regex>

#include <llvm/IR/DiagnosticInfo.h>

#include <fmt/format.h>

std::string ffi::scalar_type::as_haskell() const noexcept {
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

std::optional<ffi::scalar_type> ffi::scalar_type::from_clang(
    clang::BuiltinType::Kind type) noexcept {
  using K = clang::BuiltinType::Kind;
  const auto signedness = signedness_of(type);
  switch (type) {
    case K::Void:
      return scalar_type{None, Void, WidthNone};
    case K::Bool:
      return scalar_type{Unsigned, Bool, WidthNone};
    case K::Char_U:
    case K::Char_S:
      return scalar_type{Unspecified, Char, WidthNone};
    case K::UChar:
    case K::SChar:
      return scalar_type{signedness, Char, WidthNone};
    case K::WChar_U:
    case K::WChar_S:
      return scalar_type{Unspecified, Wchar, WidthNone};
    case K::Char8:
      return scalar_type{Unsigned, UniChar, Width8};
    case K::Char16:
      return scalar_type{Unsigned, UniChar, Width16};
    case K::Char32:
      return scalar_type{Unsigned, UniChar, Width32};
    case K::UShort:
    case K::Short:
      return scalar_type{signedness, Short, WidthNone};
    case K::UInt:
    case K::Int:
      return scalar_type{signedness, Int, WidthNone};
    case K::ULong:
    case K::Long:
      return scalar_type{signedness, Long, WidthNone};
    case K::ULongLong:
    case K::LongLong:
      return scalar_type{signedness, LLong, WidthNone};
    case K::Float:
      return scalar_type{Signed, Float, WidthNone};
    case K::Double:
      return scalar_type{Signed, Double, WidthNone};
    default:
      return std::nullopt;
  }
}

std::optional<ffi::scalar_type> ffi::scalar_type::from_name(
    std::string_view type) noexcept {
  static const std::map<std::string_view, scalar_type> scalar_types{
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

  ffi::scalar_type res{match[1].matched ? Unsigned : Signed, Special,
                       WidthNone};
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
