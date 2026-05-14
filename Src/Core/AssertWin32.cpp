#include "Assert.hpp"

#if HLX_PLATFORM_WINDOWS
#include <windows.h>

namespace hlx {
std::string GetWin32ErrorStringAnsi(unsigned long errorCode) {
  char errorString[MAX_PATH];
  ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, errorCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorString,
                   MAX_PATH, NULL);

  std::string message = "Win32 Error: ";
  message += errorString;
  return message;
}
} // namespace hlx
#endif
