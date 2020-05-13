#include "config.h"

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

bool ffi::validate_config(const config& cfg, spdlog::logger& logger) {
  if (cfg.allow_rank_n_types && !cfg.void_ptr_as_any_ptr)
    logger.warn(
        "RankNTypes is only required by void_ptr_as_any_ptr, enabling "
        "allow_rank_n_types alone adds no extra functionality, but causes "
        "incompatibility on compilers without RankNTypes support.");

  return true;
}

int ffi::name_clashes(const name_resolver::rev_name_map& m,
                      spdlog::logger& logger, std::string_view kind,
                      std::string_view scope) {
  const auto n = m.bucket_count();
  auto conflict{0};
  for (size_t i = 0; i < n; ++i) {
    if (m.bucket_size(i) <= 1) continue;
    for (auto it = m.cbegin(i); it != m.cend(i);) {
      const auto cur = it->first;
      if (next(it)->first != cur)
        ++it;
      else {
        fmt::memory_buffer buf;
        format_to(buf,
                  "the following {} names in '{}' all converts to '{}':", kind,
                  scope, it->first);
        for (; it->first == cur; ++it) format_to(buf, "\n- {}", it->second);
        logger.error(to_string(buf));
        ++conflict;
      }
    }
  }
  return conflict;
}
