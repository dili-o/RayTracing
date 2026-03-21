#pragma once

#include "Application.hpp"
#include "Renderer.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"

namespace hlx {
class PathTracer final : public Application {
public:
  void init() override;
  void run() override;
  void shutdown() override;
  void resize();

public:
  VkDeviceManager device;
  VkResourceManager rm;
  SamplerHandle fullscreen_sampler;
  VkDescriptorSet final_image_set;
  Renderer renderer;
  bool end_application;
};
} // namespace hlx
