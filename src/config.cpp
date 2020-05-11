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
      auto [l, r] = m.equal_range(it->first);
      if (next(l) != r) {
        fmt::memory_buffer buf;
        format_to(buf, "The following {} names in {} all converts to {}:\n",
                  kind, scope, it->first);
        for (; l != r; ++l) format_to(buf, "- {}\n", l->second);
        logger.error(to_string(buf));
        ++conflict;
      }
      it = r;
    }
  }
  return conflict;
}
