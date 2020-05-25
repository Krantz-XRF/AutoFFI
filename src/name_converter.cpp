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

#include <fmt/format.h>
#include <gsl/gsl_assert>
#include <gsl/gsl_util>

using namespace ffi;

namespace {

auto name_break(std::string_view input) noexcept
    -> std::optional<std::pair<std::string_view, std::string_view>> {
  if (input.empty()) return std::nullopt;
  const auto p1 = std::find_if_not(begin(input), end(input), isalnum);
  const auto p2 = [p1, input] {
    const auto alpha_upper = [](char c) { return !isalpha(c) || isupper(c); };
    if (std::all_of(begin(input), p1, alpha_upper)) return p1;
    const auto second_upper = [](char, char c) { return isupper(c); };
    return std::adjacent_find(begin(input), p1, second_upper);
  }();
  const auto d = p2 - begin(input);
  const auto p_rest = d + (p1 == p2 && p1 != end(input));
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
  // remove prefix
  if (auto [p1, p2] = std::mismatch(begin(remove_prefix), end(remove_prefix),
                                    begin(s), end(s));
      p1 == end(remove_prefix))
    s.remove_prefix(remove_prefix.length());
  // remove suffix
  if (auto [p1, p2] = std::mismatch(rbegin(remove_prefix), rend(remove_prefix),
                                    rbegin(s), rend(s));
      p1 == rend(remove_prefix))
    s.remove_suffix(remove_suffix.length());
  // add prefix/suffix
  std::string buffer = format(FMT_STRING("{}{}{}"), add_prefix, s, add_suffix);
  // convert
  if (forward_converter && !afterward)
    buffer = forward_converter->convert(buffer);
  buffer = case_convert(buffer, output_case, output_variant);
  if (forward_converter && afterward) return forward_converter->convert(buffer);
  // result
  return buffer;
}
