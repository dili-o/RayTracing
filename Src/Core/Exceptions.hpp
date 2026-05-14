#pragma once

#include <stdexcept>
namespace hlx {
class Exception : public std::runtime_error {

public:
  explicit Exception(const std::string &message)
      : std::runtime_error(message) {}
};

} // namespace hlx
