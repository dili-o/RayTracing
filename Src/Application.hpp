#pragma once

namespace hlx {

class Application {
public:
  virtual void init() = 0;
  virtual void run() = 0;
  virtual void shutdown() = 0;
};
} // namespace hlx
