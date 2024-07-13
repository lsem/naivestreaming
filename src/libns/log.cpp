#include "log.hpp"

namespace lsem_log_details {
/*extern*/ std::mutex g_lock;

const std::string& get_module(const lsem_log_details::ModuleNameDefaultTag&) {
  static const std::string no_name;
  return no_name;
}

}  // namespace lsem_log_details
