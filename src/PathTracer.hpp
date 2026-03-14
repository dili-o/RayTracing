#pragma once

#include "Application.hpp"

namespace hlx {
class PathTracer final : public Application {
public:
  void init() override;
  void run() override;
  void shutdown() override;

public:
  bool end_application;
};
} // namespace hlx
