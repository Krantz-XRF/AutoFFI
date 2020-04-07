#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace ffi {
struct Marshaller {
  enum Case {
    Case_snake_case,
    Case_Snake_case,
    Case_SNAKE_CASE,
    Case_camelCase,
    Case_PascalCase,
    Case_preserve,
  };

  using pss = std::pair<std::string_view, std::string_view>;
  static pss from_snake_case(std::string_view);
  static pss from_CamelCase(std::string_view);

  static void to_snake_case(std::string&, std::string_view);
  static void to_SNAKE_CASE(std::string&, std::string_view);
  static void to_PascalCase(std::string&, std::string_view);

  bool is_type_case() const;
  bool is_func_case() const;

  Case output_case{Case_preserve};
  std::string remove_prefix;
  std::string remove_suffix;
  std::string add_prefix;
  std::string add_suffix;
  Marshaller* forward_marshaller{nullptr};
  bool afterward{false};

  std::string transform_raw(std::string_view) const;
  std::string transform(std::string_view) const;
};

using MarshallerMap = std::map<std::string, Marshaller>;
}  // namespace ffi
