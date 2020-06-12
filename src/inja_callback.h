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

#pragma once

#include <string>
#include <tuple>
#include <type_traits>

#include <inja/environment.hpp>

namespace detail {
template <typename T>
struct is_lambda_like {
 private:
  template <typename U>
  static constexpr std::true_type test(decltype(&U::operator())) {
    return {};
  }
  template <typename U>
  static constexpr std::false_type test(...) {
    return {};
  }

 public:
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
constexpr bool is_lambda_like_v = is_lambda_like<T>::value;

template <typename T>
struct lambda_proto;

template <typename F, typename R, typename... Args>
struct lambda_proto<R (F::*)(Args...)> {
  using type = std::tuple<R, Args...>;
  static constexpr size_t arg_count = sizeof...(Args);
};

template <typename F, typename R, typename... Args>
struct lambda_proto<R (F::*)(Args...) const> {
  using type = std::tuple<R, Args...>;
  static constexpr size_t arg_count = sizeof...(Args);
};

template <typename F>
using lambda_proto_t = typename lambda_proto<decltype(&F::operator())>::type;

template <typename F>
constexpr size_t lambda_args_v =
    lambda_proto<decltype(&F::operator())>::arg_count;

template <typename F, typename Indices, typename P>
struct add_callback_helper;

template <typename F, size_t... Indices, typename R, typename... Args>
struct add_callback_helper<F, std::index_sequence<Indices...>,
                           std::tuple<R, Args...>> {
  static auto go(F f) {
    return [f](inja::Arguments& args) {
      return f(args[Indices]->template get<Args>()...);
    };
  }
};
}  // namespace detail

template <typename F, typename = std::enable_if_t<detail::is_lambda_like_v<F>>>
void add_callback(inja::Environment& env, const std::string& name, F f) {
  constexpr auto n = detail::lambda_args_v<F>;
  using ind = std::make_index_sequence<n>;
  using helper = detail::add_callback_helper<F, ind, detail::lambda_proto_t<F>>;
  env.add_callback(name, n, helper::go(f));
}

template <typename F, typename R, typename... Args>
void add_callback(inja::Environment& env, const std::string& name,
                  R (*f)(Args...)) {
  constexpr auto n = sizeof...(Args);
  using ind = std::make_index_sequence<n>;
  using helper = detail::add_callback_helper<F, ind, std::tuple<Args...>>;
  env.add_callback(name, n, helper::go(f));
}
