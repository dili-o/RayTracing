#include "Core/Log.hpp"
#include "PathTracer.hpp"

int main() {
  using namespace hlx;
  Logger logger;
  PathTracer path_tracer;
  path_tracer.init();
  path_tracer.run();
  path_tracer.shutdown();
}
