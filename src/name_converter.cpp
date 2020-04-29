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

#include "name_converter.h"

#include <algorithm>
#include <cctype>
#include <optional>

#include <gsl/gsl_assert>
#include <gsl/gsl_util>

using namespace ffi;

namespace {

auto name_break(std::string_view input) noexcept
    -> std::optional<std::pair<std::string_view, std::string_view>> {
  if (input.empty()) return std::nullopt;
  const auto p1 = std::find_if(next(begin(input)), end(input), isupper);
  const auto p2 = std::find_if_not(begin(input), end(input), isalnum);
  const auto d = min(p1, p2) - begin(input);
  const auto p_rest = d + (p1 > p2);
  return std::pair{input.substr(0, d), input.substr(p_rest)};
}

std::string case_convert_fragment(std::string_view word, name_case c) {
  Expects(!word.empty());
  std::string res{};
  switch (c) {
    case name_case::snake_all_upper:
      for (auto ch : word) res.push_back(gsl::narrow_cast<char>(toupper(ch)));
      break;
    case name_case::camel:
    case name_case::snake_init_upper:
      res.push_back(gsl::narrow_cast<char>(toupper(word[0])));
      word.remove_prefix(1);
      [[fallthrough]];
    case name_case::snake_all_lower:
      for (auto ch : word) res.push_back(gsl::narrow_cast<char>(tolower(ch)));
      break;
    case name_case::preserving:
      Expects(false);
  }
  return res;
}

std::string case_convert(std::string_view name, name_case c, name_variant v) {
  if (c == name_case::preserving) return std::string{name};
  std::string res{};
  for (std::string_view word; auto p = name_break(name);) {
    std::tie(word, name) = p.value();
    res.append(case_convert_fragment(word, c));
  }
  Expects(!res.empty());
  if (v == name_variant::preserving) return res;
  if (v == name_variant::variable)
    res[0] = gsl::narrow_cast<char>(tolower(res[0]));
  else
    res[0] = gsl::narrow_cast<char>(toupper(res[0]));
  return res;
}

}  // namespace

std::string ffi::name_converter::convert(std::string_view s) const noexcept {
  if (forward_converter && afterward) {
    const auto r = case_convert(s, output_case, output_variant);
    return forward_converter->convert(r);
  }
  if (forward_converter && !afterward) {
    const auto r = forward_converter->convert(s);
    return case_convert(r, output_case, output_variant);
  }
  return case_convert(s, output_case, output_variant);
}
