#include "VkResourceManager.hpp"
#include "Platform/FileIO.h"
#include "VkDeviceManager.h"
#include "VkShaderCompilation.h"

static const std::string base_cache_dir = "PipelineCache\\";
#if _DEBUG
static const std::string cache_sub_dir = "Debug\\";
#else
static const std::string cache_sub_dir = "Release\\";
#endif
static const std::string cache_dir = base_cache_dir + cache_sub_dir;
static const std::string cache_name = cache_dir + "cache.cache";
namespace hlx {

void VkResourceManager::init(VkDeviceManager *p_device) {
  this->p_device = p_device;

  // Create Pipeline Cache
  const bool cache_exists = FileExists(cache_name.c_str());
  std::vector<u8> cache_data;
  u64 cache_size = 0;
  if (cache_exists) {
    File cache_file(cache_name.c_str(), File::OpenRead);
    cache_size = cache_file.Size();
    if (cache_size) {
      cache_data.resize(cache_size);
      cache_file.Read(cache_size, cache_data.data());
      const VkPipelineCacheHeaderVersionOne *header =
          reinterpret_cast<VkPipelineCacheHeaderVersionOne *>(
              cache_data.data());
      HASSERT(header);

      if (header->headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE ||
          header->vendorID !=
              p_device->vk_physical_device_properties.vendorID ||
          header->deviceID !=
              p_device->vk_physical_device_properties.deviceID ||
          memcmp(header->pipelineCacheUUID,
                 p_device->vk_physical_device_properties.pipelineCacheUUID,
                 VK_UUID_SIZE) != 0) {
        // The cache is invalid for this driver/hardware.
        // Discard it and create a fresh one.
        cache_size = 0;
      }
    }
  } else {
    if (DirectoryExists(base_cache_dir.c_str()) == false)
      CreateNewDirectory(base_cache_dir.c_str());

    if (DirectoryExists(cache_dir.c_str()) == false)
      CreateNewDirectory(cache_dir.c_str());

    File cache_file(cache_name.c_str(), File::OpenWrite);
  }
  const VkPipelineCacheCreateInfo cache_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .initialDataSize = cache_size,
      .pInitialData =
          cache_data.data() // NOTE: This is ignored if initialDataSize is zero
  };

  VK_CHECK(vkCreatePipelineCache(p_device->vk_device, &cache_info, nullptr,
                                 &vk_pipeline_cache));

  buffer_pool.init(10);
  image_pool.init(10);
  image_view_pool.init(10);
  pipeline_pool.init(10);
  sampler_pool.init(10);
  shader_pool.init(10);
  set_layout_pool.init(10);
}

void VkResourceManager::update(u64 current_frame_index) {
  for (int i = deletion_entries.size() - 1; i >= 0; --i) {
    const DeletionEntry &entry = deletion_entries[i];

    if ((current_frame_index - entry.frame_index) > MAX_FRAMES_IN_FLIGHT) {
      std::visit(
          [&](auto &&handle) {
            using T = std::decay_t<decltype(handle)>;
            if constexpr (std::is_same_v<T, BufferHandle>) {
              VulkanBuffer *buffer = access_buffer(handle);
              if (!buffer)
                return;
              if (buffer->p_data) {
                vmaUnmapMemory(p_device->vma_allocator, buffer->vma_allocation);
              }
              vmaDestroyBuffer(p_device->vma_allocator, buffer->vk_handle,
                               buffer->vma_allocation);
              buffer_pool.release(handle);
            } else if constexpr (std::is_same_v<T, ImageHandle>) {
              VulkanImage *image = access_image(handle);
              if (!image)
                return;
              if (image->view_count == 0) {
                vmaDestroyImage(p_device->vma_allocator, image->vk_handle,
                                image->vma_allocation);
                image_pool.release(handle);
              }
            } else if constexpr (std::is_same_v<T, ImageViewHandle>) {
              VulkanImageView *image_view = access_image_view(handle);
              if (!image_view)
                return;
              VulkanImage *image = access_image(image_view->image_handle);
              HASSERT(image);
              if (--image->view_count == 0) {
                vmaDestroyImage(p_device->vma_allocator, image->vk_handle,
                                image->vma_allocation);
                image_pool.release(image_view->image_handle);
              }
              vkDestroyImageView(p_device->vk_device, image_view->vk_handle,
                                 nullptr);
              image_view_pool.release(handle);
            } else if constexpr (std::is_same_v<T, PipelineHandle>) {
              VulkanPipeline *pipeline = access_pipeline(handle);
              if (!pipeline)
                return;
              vkDestroyPipelineLayout(p_device->vk_device,
                                      pipeline->vk_pipeline_layout, nullptr);
              vkDestroyPipeline(p_device->vk_device, pipeline->vk_handle,
                                nullptr);
              pipeline_pool.release(handle);
            } else if constexpr (std::is_same_v<T, SamplerHandle>) {
              VulkanSampler *sampler = access_sampler(handle);
              if (!sampler)
                return;
              vkDestroySampler(p_device->vk_device, sampler->vk_handle,
                               nullptr);
              sampler_pool.release(handle);
            } else if constexpr (std::is_same_v<T, ShaderHandle>) {
              VulkanShader *shader = access_shader(handle);
              if (!shader)
                return;
              vkDestroyShaderModule(p_device->vk_device, shader->vk_handle,
                                    nullptr);
              shader_pool.release(handle);
            } else {
              VulkanSetLayout *set_layout = access_set_layout(handle);
              if (!set_layout)
                return;
              vkDestroyDescriptorSetLayout(p_device->vk_device,
                                           set_layout->vk_handle, nullptr);
              set_layout_pool.release(handle);
            }
          },
          entry.handle);
      std::swap(deletion_entries[i], deletion_entries.back());
      deletion_entries.pop_back();
    }
  }
}

// Create functions
BufferHandle VkResourceManager::create_buffer(
    std::string_view name, VkBufferCreateInfo &create_info,
    const VmaAllocationCreateInfo &vma_create_info) {
  BufferHandle handle = buffer_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan buffer Resource!");
    return handle;
  }

  VulkanBuffer *buffer = buffer_pool.obtain(handle);
  *buffer = {};

  if (create_info.size == 0) {
    HWARN("VkBufferCreateInfo.size was set to 0, creating {} with a size of 4 "
          "instead",
          name);
    create_info.size = 4;
  }
  VK_CHECK(vmaCreateBuffer(p_device->vma_allocator, &create_info,
                           &vma_create_info, &buffer->vk_handle,
                           &buffer->vma_allocation, nullptr));
  p_device->set_resource_name<VkBuffer>(VK_OBJECT_TYPE_BUFFER,
                                        buffer->vk_handle, name);
  if (vma_create_info.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
    vmaMapMemory(p_device->vma_allocator, buffer->vma_allocation,
                 &buffer->p_data);
  }

  if (create_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo address_info{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    address_info.buffer = buffer->vk_handle;
    buffer->vk_device_address =
        vkGetBufferDeviceAddress(p_device->vk_device, &address_info);
  }

  buffer->vk_device_size = create_info.size;

  return handle;
}

ImageHandle VkResourceManager::create_image(
    std::string_view name, const VkImageCreateInfo &image_create_info,
    const VmaAllocationCreateInfo &vma_create_info) {
  ImageHandle handle = image_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan image Resource!");
    return handle;
  }

  VulkanImage *image = image_pool.obtain(handle);
  *image = {};

  VK_CHECK(vmaCreateImage(p_device->vma_allocator, &image_create_info,
                          &vma_create_info, &image->vk_handle,
                          &image->vma_allocation, nullptr));
  p_device->set_resource_name<VkImage>(VK_OBJECT_TYPE_IMAGE, image->vk_handle,
                                       name);

  image->vk_extent.width = image_create_info.extent.width;
  image->vk_extent.height = image_create_info.extent.height;
  image->vk_format = image_create_info.format;

  return handle;
}

ImageViewHandle
VkResourceManager::create_image_view(std::string_view name,
                                     ImageHandle image_handle,
                                     VkImageViewCreateInfo &view_create_info) {
  ImageViewHandle handle = image_view_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan image view Resource!");
    return handle;
  }

  VulkanImageView *image_view = image_view_pool.obtain(handle);
  *image_view = {};

  image_view->image_handle = image_handle;

  // Try to obtain image
  VulkanImage *image = image_pool.obtain(image_handle);
  HASSERT_MSG(image, "image_handle is invalid");

  view_create_info.image = image->vk_handle;
  ++image->view_count;
  VK_CHECK(vkCreateImageView(p_device->vk_device, &view_create_info, nullptr,
                             &image_view->vk_handle));
  p_device->set_resource_name<VkImageView>(VK_OBJECT_TYPE_IMAGE_VIEW,
                                           image_view->vk_handle, name);

  image_view->base_level = view_create_info.subresourceRange.baseMipLevel;
  image_view->level_count = view_create_info.subresourceRange.levelCount;
  image_view->base_layer = view_create_info.subresourceRange.baseArrayLayer;
  image_view->layer_count = view_create_info.subresourceRange.layerCount;

  return handle;
}

ImageViewHandle VkResourceManager::create_image_view(
    std::string_view view_name, std::string_view image_name,
    const VkImageCreateInfo &image_create_info,
    const VmaAllocationCreateInfo &vma_create_info,
    VkImageViewCreateInfo &view_create_info) {
  return create_image_view(
      view_name, create_image(image_name, image_create_info, vma_create_info),
      view_create_info);
}

PipelineHandle VkResourceManager::create_graphics_pipeline(
    std::string_view name, VkGraphicsPipelineCreateInfo &pipeline_info,
    const VkPipelineLayoutCreateInfo &pipeline_layout_info) {
  PipelineHandle handle = pipeline_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan pipeline Resource!");
    return handle;
  }

  VulkanPipeline *pipeline = pipeline_pool.obtain(handle);
  *pipeline = {};

  // TODO: Pipeline layout caching
  VK_CHECK(vkCreatePipelineLayout(p_device->vk_device, &pipeline_layout_info,
                                  nullptr, &pipeline->vk_pipeline_layout));
  pipeline_info.layout = pipeline->vk_pipeline_layout;
  std::string pipeline_layout_name = std::string(name) + "Layout";
  p_device->set_resource_name<VkPipelineLayout>(VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                                pipeline->vk_pipeline_layout,
                                                pipeline_layout_name);

  VK_CHECK(vkCreateGraphicsPipelines(p_device->vk_device, vk_pipeline_cache, 1,
                                     &pipeline_info, nullptr,
                                     &pipeline->vk_handle));
  p_device->set_resource_name<VkPipeline>(VK_OBJECT_TYPE_PIPELINE,
                                          pipeline->vk_handle, name);

  return handle;
}

PipelineHandle VkResourceManager::create_compute_pipeline(
    std::string_view name, VkComputePipelineCreateInfo &pipeline_info,
    const VkPipelineLayoutCreateInfo &pipeline_layout_info) {
  PipelineHandle handle = pipeline_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan pipeline Resource!");
    return handle;
  }

  VulkanPipeline *pipeline = pipeline_pool.obtain(handle);
  *pipeline = {};

  // TODO: Pipeline layout caching
  VK_CHECK(vkCreatePipelineLayout(p_device->vk_device, &pipeline_layout_info,
                                  nullptr, &pipeline->vk_pipeline_layout));
  pipeline_info.layout = pipeline->vk_pipeline_layout;
  std::string pipeline_layout_name = std::string(name) + "Layout";
  p_device->set_resource_name<VkPipelineLayout>(VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                                pipeline->vk_pipeline_layout,
                                                pipeline_layout_name);

  VK_CHECK(vkCreateComputePipelines(p_device->vk_device, vk_pipeline_cache, 1,
                                    &pipeline_info, nullptr,
                                    &pipeline->vk_handle));
  p_device->set_resource_name<VkPipeline>(VK_OBJECT_TYPE_PIPELINE,
                                          pipeline->vk_handle, name);

  return handle;
}

SamplerHandle VkResourceManager::create_sampler(
    std::string_view name, const VkSamplerCreateInfo &sampler_create_info) {
  SamplerHandle handle = sampler_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan sampler Resource!");
    return handle;
  }

  VulkanSampler *sampler = sampler_pool.obtain(handle);
  *sampler = {};

  VK_CHECK(vkCreateSampler(p_device->vk_device, &sampler_create_info, nullptr,
                           &sampler->vk_handle));
  p_device->set_resource_name<VkSampler>(VK_OBJECT_TYPE_SAMPLER,
                                         sampler->vk_handle, name);

  return handle;
}

ShaderHandle VkResourceManager::create_shader(std::string_view name,
                                              const ShaderBlob &blob) {
  ShaderHandle handle = shader_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan shader Resource!");
    return handle;
  }

  VulkanShader *shader = shader_pool.obtain(handle);
  *shader = {};

  VkShaderModuleCreateInfo create_info{
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  create_info.codeSize = blob.size();
  create_info.pCode = blob.data();

  VK_CHECK(vkCreateShaderModule(p_device->vk_device, &create_info, nullptr,
                                &shader->vk_handle));
  p_device->set_resource_name<VkShaderModule>(VK_OBJECT_TYPE_SHADER_MODULE,
                                              shader->vk_handle, name);

  return handle;
}

SetLayoutHandle VkResourceManager::create_descriptor_set_layout(
    std::string_view name,
    const VkDescriptorSetLayoutCreateInfo &set_layout_info) {
  SetLayoutHandle handle = set_layout_pool.obtain_new();
  if (!is_handle_valid(handle)) {
    HERROR("Failed to obtain a Vulkan set layout Resource!");
    return handle;
  }

  VulkanSetLayout *set_layout = set_layout_pool.obtain(handle);
  *set_layout = {};

  VK_CHECK(vkCreateDescriptorSetLayout(p_device->vk_device, &set_layout_info,
                                       nullptr, &set_layout->vk_handle));
  p_device->set_resource_name<VkDescriptorSetLayout>(
      VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, set_layout->vk_handle, name);

  return handle;
}

void VkResourceManager::queue_destroy(DeletionEntry entry) {
  deletion_entries.push_back(entry);
}

void VkResourceManager::shutdown() {
  // Update with max uint64 to delete all queued handles
  vkDeviceWaitIdle(p_device->vk_device);
  update(UINT64_MAX);

  // Save Pipeline Cache
  size_t cache_size = 0;
  VK_CHECK(vkGetPipelineCacheData(p_device->vk_device, vk_pipeline_cache,
                                  &cache_size, nullptr));
  if (cache_size) {
    std::vector<u8> cache_data(cache_size);
    VK_CHECK(vkGetPipelineCacheData(p_device->vk_device, vk_pipeline_cache,
                                    &cache_size, cache_data.data()));
    File cache_file(cache_name.c_str(), File::OpenWrite);

    // Write the compiled shader to disk
    cache_file.Write(cache_size, cache_data.data());
  }
  vkDestroyPipelineCache(p_device->vk_device, vk_pipeline_cache, nullptr);

  buffer_pool.shutdown();
  image_pool.shutdown();
  image_view_pool.shutdown();
  pipeline_pool.shutdown();
  sampler_pool.shutdown();
  shader_pool.shutdown();
  set_layout_pool.shutdown();

  p_device = nullptr;
}
} // namespace hlx
