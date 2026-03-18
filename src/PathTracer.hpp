#pragma once

#include "Application.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"

namespace hlx {
class PathTracer final : public Application {
public:
  void init() override;
  void run() override;
  void shutdown() override;

public:
  VkDeviceManager device;
  VkResourceManager resource_manager;
  bool end_application;
};
} // namespace hlx
