#pragma once

#include <format>
#include <iostream>
#include <string_view>

namespace lsem_log_details {
enum class LogLevel { debug, info, warning, error };
template <class... Args>
inline void print_log(LogLevel level, std::string_view fmt, Args&&... args) {
  auto label_fn = [](LogLevel level) {
    switch (level) {
      case LogLevel::debug:
        return "DEBUG";
      case LogLevel::info:
        return "INFO";
      case LogLevel::warning:
        return "WARNING";
      case LogLevel::error:
        return "ERROR";
      default:
        return "LogLevel::<unknown>";
    }
  };
  std::cout << label_fn(level) << ": "
            << std::vformat(fmt, std::make_format_args(args...)) << "\n";
}

}  // namespace lsem_log_details
#define LOG_DEBUG(FmtMsg, ...)                                           \
  lsem_log_details::print_log(lsem_log_details::LogLevel::debug, FmtMsg, \
                              ##__VA_ARGS__)
#define LOG_INFO(FmtMsg, ...)                                           \
  lsem_log_details::print_log(lsem_log_details::LogLevel::info, FmtMsg, \
                              ##__VA_ARGS__)
#define LOG_WARNING(FmtMsg, ...)                                           \
  lsem_log_details::print_log(lsem_log_details::LogLevel::warning, FmtMsg, \
                              ##__VA_ARGS__)
#define LOG_ERROR(FmtMsg, ...)                                           \
  lsem_log_details::print_log(lsem_log_details::LogLevel::error, FmtMsg, \
                              ##__VA_ARGS__)
