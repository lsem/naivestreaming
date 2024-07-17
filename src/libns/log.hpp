#pragma once

#include <format>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string_view>

namespace lsem_log_details {
extern std::mutex g_lock;

enum class LogLevel { debug, info, warning, error };

constexpr auto MODULE_WIDTH = 10;

class ModuleNameDefaultTag {};
class ModuleNameSpecificTag : public ModuleNameDefaultTag {};
const std::string& get_module(const ModuleNameDefaultTag&);
// Defines module name. Should be a valid C++ identifier as the name used in
// class name generation.
#define LOG_MODULE_NAME(Name)                                     \
  namespace {                                                     \
  const std::string& get_module(                                  \
      const lsem_log_details::ModuleNameSpecificTag& m) {         \
    static const std::string this_module_name{std::string{Name} + \
                                              std::string(": ")}; \
    return this_module_name;                                      \
  }                                                               \
  }

template <class... Args>
inline void print_log(LogLevel level,
                      const std::string& module_,
                      std::string_view fmt,
                      Args&&... args) {
  auto label_fn = [](LogLevel level) {
    switch (level) {
      case LogLevel::debug:
        return "  DEBUG";
      case LogLevel::info:
        return "   INFO";
      case LogLevel::warning:
        return "WARNING";
      case LogLevel::error:
        return "ERROR ";
      default:
        return "LogLevel::<unknown>";
    }
  };

  auto s = std::vformat(fmt, std::make_format_args(args...));
  std::lock_guard lck{g_lock};
  std::cout << label_fn(level) << ": " << std::setw(MODULE_WIDTH) << module_
            << s << "\n";
}

}  // namespace lsem_log_details
#define LOG_DEBUG(FmtMsg, ...)                                       \
  lsem_log_details::print_log(                                       \
      lsem_log_details::LogLevel::debug,                             \
      get_module(lsem_log_details::ModuleNameSpecificTag{}), FmtMsg, \
      ##__VA_ARGS__)
#define LOG_INFO(FmtMsg, ...)                                        \
  lsem_log_details::print_log(                                       \
      lsem_log_details::LogLevel::info,                              \
      get_module(lsem_log_details::ModuleNameSpecificTag{}), FmtMsg, \
      ##__VA_ARGS__)
#define LOG_WARNING(FmtMsg, ...)                                     \
  lsem_log_details::print_log(                                       \
      lsem_log_details::LogLevel::warning,                           \
      get_module(lsem_log_details::ModuleNameSpecificTag{}), FmtMsg, \
      ##__VA_ARGS__)
#define LOG_ERROR(FmtMsg, ...)                                       \
  lsem_log_details::print_log(                                       \
      lsem_log_details::LogLevel::error,                             \
      get_module(lsem_log_details::ModuleNameSpecificTag{}), FmtMsg, \
      ##__VA_ARGS__)
