#pragma once

#include "VkResources.hpp"

namespace hlx {

struct VkDeviceManager;
struct VkResourceManager;

struct VkStagingBuffer {
public:
  void init(VkDeviceManager *p_device, VkResourceManager *p_manager,
            u32 queue_family_index, VkQueue vk_queue, size_t size);
  void shutdown();

  void flush();
  void stage(const void *p_data, BufferHandle dst_buffer, size_t dst_offset,
             size_t size);
  // NOTE: This function assumes the texture has been recently made and
  // is in UNDEFINED layout. It also assumes that it is a SAMPLED texture
  void stage(const void *p_data, ImageViewHandle image_view, size_t size,
             size_t alignment = 1);

  // Just copy data to the buffer
  void copy_data(const void *p_data, size_t size, size_t alignment = 1);

public:
  VkDeviceManager *p_device{nullptr};
  VkResourceManager *p_resource_manager{nullptr};
  BufferHandle buffer_handle;
  VkCommandPool vk_command_pool{VK_NULL_HANDLE};
  VkCommandBuffer vk_command_buffer{VK_NULL_HANDLE};
  VkQueue vk_queue{VK_NULL_HANDLE};
  bool is_recording{false};

private:
  void begin();
  void end();
};
} // namespace hlx
