#pragma once

#include "VkResources.hpp"
#include <variant>

namespace hlx {
struct VkDeviceManager;
struct ShaderBlob;

using DestroyHandle =
    std::variant<BufferHandle, ImageHandle, ImageViewHandle, PipelineHandle,
                 SamplerHandle, ShaderHandle, SetLayoutHandle>;
struct DeletionEntry {
  DestroyHandle handle;
  u64 frame_index;
};

struct VkResourceManager {
public:
  void init(VkDeviceManager *p_device);
  void shutdown();
  void update(u64 current_frame_index);

  // Create functions
  BufferHandle create_buffer(std::string_view name,
                             VkBufferCreateInfo &create_info,
                             const VmaAllocationCreateInfo &vma_create_info);
  ImageHandle create_image(std::string_view name,
                           const VkImageCreateInfo &image_create_info,
                           const VmaAllocationCreateInfo &vma_create_info);
  ImageViewHandle create_image_view(std::string_view name,
                                    ImageHandle image_handle,
                                    VkImageViewCreateInfo &view_create_info);
  PipelineHandle create_graphics_pipeline(
      std::string_view name, VkGraphicsPipelineCreateInfo &pipeline_info,
      const VkPipelineLayoutCreateInfo &pipeline_layout_info);
  PipelineHandle create_compute_pipeline(
      std::string_view name, VkComputePipelineCreateInfo &pipeline_info,
      const VkPipelineLayoutCreateInfo &pipeline_layout_info);
  SamplerHandle create_sampler(std::string_view name,
                               const VkSamplerCreateInfo &sampler_create_info);
  ShaderHandle create_shader(std::string_view name, const ShaderBlob &blob);
  SetLayoutHandle create_descriptor_set_layout(
      std::string_view name,
      const VkDescriptorSetLayoutCreateInfo &set_layout_info);

  // Destroy functions
  void queue_destroy(DeletionEntry entry);

  // Access funtions
  inline VulkanBuffer *access_buffer(BufferHandle handle) {
    return buffer_pool.obtain(handle);
  }
  inline VulkanImage *access_image(ImageHandle handle) {
    return image_pool.obtain(handle);
  }
  inline VulkanImageView *access_image_view(ImageViewHandle handle) {
    return image_view_pool.obtain(handle);
  }
  inline VulkanPipeline *access_pipeline(PipelineHandle handle) {
    return pipeline_pool.obtain(handle);
  }
  inline VulkanSampler *access_sampler(SamplerHandle handle) {
    return sampler_pool.obtain(handle);
  }
  inline VulkanShader *access_shader(ShaderHandle handle) {
    return shader_pool.obtain(handle);
  }
  inline VulkanSetLayout *access_set_layout(SetLayoutHandle handle) {
    return set_layout_pool.obtain(handle);
  }

public:
  VkDeviceManager *p_device;
  VkPipelineCache vk_pipeline_cache{VK_NULL_HANDLE};
  std::vector<DeletionEntry> deletion_entries;
  ResourcePool<BufferHandle, VulkanBuffer> buffer_pool;
  ResourcePool<ImageHandle, VulkanImage> image_pool;
  ResourcePool<ImageViewHandle, VulkanImageView> image_view_pool;
  ResourcePool<PipelineHandle, VulkanPipeline> pipeline_pool;
  ResourcePool<SamplerHandle, VulkanSampler> sampler_pool;
  ResourcePool<ShaderHandle, VulkanShader> shader_pool;
  ResourcePool<SetLayoutHandle, VulkanSetLayout> set_layout_pool;
};
} // namespace hlx
