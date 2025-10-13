
#pragma once

#include "Defines.hpp"

#include <memory>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace hlx {
class HLX_API Logger {
public:
  Logger();
  ~Logger();

  static std::shared_ptr<spdlog::logger> &GetCoreLogger();

  // Core log macros
#ifdef _DEBUG
#define HDEBUG(...) hlx::Logger::GetCoreLogger()->debug(__VA_ARGS__)
#else
#define HDEBUG(...)
#endif
#define HTRACE(...) hlx::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define HINFO(...) hlx::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define HWARN(...) hlx::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define HERROR(...) hlx::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define HCRITICAL(...)                                                         \
  hlx::Logger::GetCoreLogger()->critical(__VA_ARGS__);                         \
  __debugbreak();
#define HCRITICAL_NO_BREAK(...)                                                \
  hlx::Logger::GetCoreLogger()->critical(__VA_ARGS__)
};
} // namespace hlx
