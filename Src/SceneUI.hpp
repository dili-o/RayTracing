#pragma once

#include "Vulkan/VkResources.hpp"

namespace hlx {
struct VkDeviceManager;
struct VkResourceManager;
struct VkStagingBuffer;

struct SceneUI {
public:
  void init(VkDeviceManager *p_device, VkResourceManager *p_rm,
            VkStagingBuffer &staging_buffer);
  void shutdown();

  bool handle_events(void *event);
  void begin_frame();
  void end_frame();

public:
  VkDeviceManager *p_device;
  VkResourceManager *p_rm;
  ImageViewHandle font_texture_view;
  PipelineHandle pipeline;
  SetLayoutHandle set_layout;
  VkDescriptorSet set;
  SamplerHandle linear_sampler;
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> vertex_buffers;
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> index_buffers;
};
} // namespace hlx
