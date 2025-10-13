
#include "Log.hpp"
#include "Defines.hpp"

namespace hlx {
std::shared_ptr<spdlog::logger> s_CoreLogger;

Logger::Logger() {
  spdlog::set_pattern("%^[%T] %n [%l]: %v%$");
  s_CoreLogger = spdlog::stdout_color_mt("Helix Engine");
  s_CoreLogger->set_level(spdlog::level::trace);
  HINFO("Logger initialised");
}

Logger::~Logger() { HINFO("Logger destroyed"); }

inline std::shared_ptr<spdlog::logger> &Logger::GetCoreLogger() {
  return s_CoreLogger;
}

void ReportAssertionFailure(cstring expression, cstring message, cstring file,
                            i32 line) {
  HCRITICAL("Assertion Failure: {}, message: {}, in file: {}, line: {}",
            expression, message, file, line);
}
} // namespace hlx
