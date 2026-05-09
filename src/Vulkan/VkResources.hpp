#pragma once

#include "Core/ResourcePool.hpp"
// Vendor
#include <vk_mem_alloc.h>
#include <volk/volk.h>
#ifdef HELIX_WITH_CUDA
#include <cuda_runtime.h>
#endif // HELIX_WITH_CUDA

namespace hlx {
struct BufferTag {};
struct PipelineTag {};
struct ImageTag {};
struct ImageViewTag {};
struct SamplerTag {};
struct ShaderTag {};
struct SetLayoutTag {};

using BufferHandle = ResourceHandle<BufferTag>;
using PipelineHandle = ResourceHandle<PipelineTag>;
using ImageHandle = ResourceHandle<ImageTag>;
using ImageViewHandle = ResourceHandle<ImageViewTag>;
using SamplerHandle = ResourceHandle<SamplerTag>;
using ShaderHandle = ResourceHandle<ShaderTag>;
using SetLayoutHandle = ResourceHandle<SetLayoutTag>;

struct VulkanBuffer {
public:
  VkBuffer vk_handle{VK_NULL_HANDLE};
  VkDeviceSize current_size{0};
  VkDeviceSize vk_device_size{0};
  VmaAllocation vma_allocation{VK_NULL_HANDLE};
  VkDeviceAddress vk_device_address{0};
  void *p_data{nullptr};
#ifdef HELIX_WITH_CUDA
  cudaExternalMemory_t cu_ext_mem{0};
  void *cu_dev_ptr{nullptr};
#endif
};

struct VulkanImage {
public:
  inline u32 width() const noexcept { return vk_extent.width; }
  inline u32 height() const noexcept { return vk_extent.height; };

public:
  VkImage vk_handle{VK_NULL_HANDLE};
  VmaAllocation vma_allocation{VK_NULL_HANDLE};
  VkFormat vk_format{VK_FORMAT_UNDEFINED};
  VkExtent2D vk_extent;
  VkImageLayout current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkPipelineStageFlags2 current_stage{VK_PIPELINE_STAGE_2_NONE};
  VkAccessFlags2 current_access{VK_ACCESS_2_NONE};
  u32 view_count{0};
#ifdef HELIX_WITH_CUDA
  cudaTextureObject_t cu_texture_obj{0};
  cudaArray_t cu_native_array{0};
#endif // HELIX_WITH_CUDA
};

struct VulkanImageView {
public:
  VkImageView vk_handle{VK_NULL_HANDLE};
  ImageHandle image_handle;
  u32 base_level{0};
  u32 level_count{1};
  u32 base_layer{0};
  u32 layer_count{1};
};

struct VulkanPipeline {
public:
  VkPipeline vk_handle{VK_NULL_HANDLE};
  VkPipelineLayout vk_pipeline_layout{VK_NULL_HANDLE};
};

struct VulkanSampler {
public:
  VkSampler vk_handle{VK_NULL_HANDLE};
};

struct VulkanShader {
public:
  VkShaderModule vk_handle{VK_NULL_HANDLE};
};

struct VulkanSetLayout {
public:
  VkDescriptorSetLayout vk_handle{VK_NULL_HANDLE};
};

} // namespace hlx
