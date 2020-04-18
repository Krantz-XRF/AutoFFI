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

#include "marshaller.h"

#include <algorithm>
#include <array>
#include <cctype>

auto ffi::marshaller::from_snake_case(std::string_view name) -> pss {
  auto notalnum = [](char c) { return !isalnum(c); };
  auto p = std::find_if(name.cbegin(), name.cend(), notalnum);
  if (p == name.cend()) return {name, {}};
  auto d = p - name.cbegin();
  return {name.substr(0, d), name.substr(d + 1)};
}

auto ffi::marshaller::from_CamelCase(std::string_view name) -> pss {
  auto p = std::find_if(std::next(name.cbegin()), name.cend(), isupper);
  if (p == name.cend()) return {name, {}};
  auto d = p - name.cbegin();
  return {name.substr(0, d), name.substr(d)};
}

void ffi::marshaller::to_snake_case(std::string& res, std::string_view s) {
  if (!res.empty()) res.push_back('_');
  std::transform(s.cbegin(), s.cend(), std::back_inserter(res), tolower);
}

void ffi::marshaller::to_SNAKE_CASE(std::string& res, std::string_view s) {
  if (!res.empty()) res.push_back('_');
  std::transform(s.cbegin(), s.cend(), std::back_inserter(res), toupper);
}

void ffi::marshaller::to_PascalCase(std::string& res, std::string_view s) {
  if (s.empty()) return;
  res.push_back(toupper(s[0]));
  std::transform(s.cbegin() + 1, s.cend(), std::back_inserter(res), tolower);
}

bool ffi::marshaller::is_type_case() const {
  if (add_prefix.empty())
    return output_case == Case_PascalCase || output_case == Case_SNAKE_CASE ||
           output_case == Case_Snake_case;
  else
    return isupper(add_prefix[0]);
}

bool ffi::marshaller::is_func_case() const {
  if (add_prefix.empty())
    return output_case == Case_camelCase || output_case == Case_snake_case;
  else
    return islower(add_prefix[0]) || add_prefix[0] == '_';
}

std::string ffi::marshaller::transform_raw(std::string_view name) const {
  if (name.length() >= remove_prefix.length() &&
      std::equal(remove_prefix.cbegin(), remove_prefix.cend(), name.cbegin()))
    name.remove_prefix(remove_prefix.length());
  if (name.length() >= remove_suffix.length() &&
      std::equal(remove_suffix.crbegin(), remove_suffix.crend(),
                 name.crbegin()))
    name.remove_suffix(remove_suffix.length());

  constexpr std::array<pss (*)(std::string_view), 2> split{
      from_snake_case,
      from_CamelCase,
  };
  constexpr std::array<void (*)(std::string&, std::string_view), 5> consume{
      to_snake_case, to_snake_case, to_SNAKE_CASE, to_PascalCase, to_PascalCase,
  };

  auto input_case = std::none_of(name.cbegin(), name.cend(),
                                 [](char c) { return !isalnum(c); });

  std::string res{add_prefix};

  if (output_case == Case_preserve)
    res.append(name);
  else {
    auto input = name;
    while (!input.empty()) {
      std::string_view fraction;
      std::tie(fraction, input) = split[input_case](input);
      consume[output_case](res, fraction);
    }

    if (output_case == Case_Snake_case)
      res[0] = toupper(res[0]);
    else if (output_case == Case_camelCase)
      res[0] = tolower(res[0]);
  }

  res.append(add_suffix);
  return res;
}

std::string ffi::marshaller::transform(std::string_view name) const {
  if (forward_marshaller && afterward)
    return forward_marshaller->transform(transform_raw(name));
  else if (forward_marshaller)
    return transform_raw(forward_marshaller->transform(name));
  else
    return transform_raw(name);
}
