#include "VkStagingBuffer.h"
#include "Core/Assert.hpp"
#include "PCH.h"
#include "VkDeviceManager.h"
#include "VkErr.h"
#include "VkResourceManager.hpp"

namespace hlx {

void VkStagingBuffer::shutdown() {
  if (!p_device) {
    return;
  }

  vkDestroyCommandPool(p_device->vk_device, vk_command_pool, nullptr);

  p_resource_manager->queue_destroy({buffer_handle, 0});

  p_device = nullptr;
  p_resource_manager = nullptr;
  vk_command_pool = VK_NULL_HANDLE;
  vk_command_buffer = VK_NULL_HANDLE;
  is_recording = false;
}

void VkStagingBuffer::begin() {
  if (is_recording)
    return;
  VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(vk_command_buffer, &begin_info));
  is_recording = true;
}

void VkStagingBuffer::end() {
  if (!is_recording)
    return;

  VK_CHECK(vkEndCommandBuffer(vk_command_buffer));

  is_recording = false;
}

void VkStagingBuffer::init(VkDeviceManager *p_device,
                           VkResourceManager *p_manager, u32 queue_family_index,
                           VkQueue vk_queue, size_t size) {
  HASSERT(p_device);
  HASSERT(p_manager);
  if (p_manager->access_buffer(buffer_handle)) {
    std::printf("VkStagingBuffer has already been initialized!\n");
    return;
  }

  this->p_device = p_device;
  this->p_resource_manager = p_manager;
  this->vk_queue = vk_queue;

  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo vma_info{};
  vma_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vma_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  buffer_handle =
      p_resource_manager->create_buffer("StagingBuffer", buffer_info, vma_info);

  VkCommandPoolCreateInfo command_pool_create_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  command_pool_create_info.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_create_info.queueFamilyIndex = queue_family_index;
  VK_CHECK(vkCreateCommandPool(p_device->vk_device, &command_pool_create_info,
                               nullptr, &vk_command_pool));

  VkCommandBufferAllocateInfo cb_alloc_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cb_alloc_info.commandPool = vk_command_pool;
  cb_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_alloc_info.commandBufferCount = 1;
  VK_CHECK(vkAllocateCommandBuffers(p_device->vk_device, &cb_alloc_info,
                                    &vk_command_buffer));
}

void VkStagingBuffer::stage(const void *p_data, BufferHandle dst_buffer_handle,
                            size_t dst_offset, size_t size) {
  if (size == 0)
    return;

  begin();

  VulkanBuffer *buffer = p_resource_manager->access_buffer(buffer_handle);
  // Data to transfer cannot fit inside the staging buffer
  if ((buffer->current_size + size) > buffer->vk_device_size) {
    const size_t remaining_size = buffer->vk_device_size - buffer->current_size;
    stage(p_data, dst_buffer_handle, dst_offset, remaining_size);
    flush();
    dst_offset += remaining_size;
    size -= remaining_size;
    const u8 *p_remaining_data =
        static_cast<const u8 *>(p_data) + remaining_size;
    stage(p_remaining_data, dst_buffer_handle, dst_offset, size);
  } else {
    VulkanBuffer *dst_buffer =
        p_resource_manager->access_buffer(dst_buffer_handle);
    if (dst_buffer->vk_device_size < (dst_offset + size)) {
      HASSERT_MSG(false, "VkStagingBuffer::stage() - Destination buffer size "
                         "is less than the size of the data to copy");
    }
    std::memcpy(static_cast<u8 *>(buffer->p_data) + buffer->current_size,
                p_data, size);

    VkBufferCopy2 region{VK_STRUCTURE_TYPE_BUFFER_COPY_2};
    region.srcOffset = buffer->current_size;
    region.dstOffset = dst_offset;
    region.size = size;

    VkCopyBufferInfo2 buffer_info{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
    buffer_info.srcBuffer = buffer->vk_handle;
    buffer_info.dstBuffer = dst_buffer->vk_handle;
    buffer_info.regionCount = 1;
    buffer_info.pRegions = &region;

    vkCmdCopyBuffer2(vk_command_buffer, &buffer_info);

    buffer->current_size += size;
    dst_buffer->current_size =
        std::max(dst_buffer->current_size, (dst_offset + size));
  }
}

void VkStagingBuffer::stage(const void *p_data,
                            ImageViewHandle image_view_handle, size_t size,
                            size_t alignment) {
  begin();

  VulkanBuffer *buffer = p_resource_manager->access_buffer(buffer_handle);
  // Align Up
  buffer->current_size =
      (buffer->current_size + alignment - 1) & ~(alignment - 1);

  if ((buffer->current_size + size) > buffer->vk_device_size) {
    if (size > buffer->vk_device_size) {
      throw std::out_of_range("VkStagingBuffer::Stage - Data size (" +
                              std::to_string(size) +
                              ") exceeds buffer buffer->vk_device_size (" +
                              std::to_string(buffer->vk_device_size) + ")");
    } else {
      flush();
      stage(p_data, image_view_handle, size);
      return;
    }
  }

  std::memcpy(static_cast<u8 *>(buffer->p_data) + buffer->current_size, p_data,
              size);

  // Transition image to transfer dst
  VulkanImageView *image_view =
      p_resource_manager->access_image_view(image_view_handle);
  VulkanImage *image =
      p_resource_manager->access_image(image_view->image_handle);
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
  image_barrier.srcAccessMask = VK_ACCESS_2_NONE;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier.image = image->vk_handle;
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = image_view->base_level;
  image_barrier.subresourceRange.levelCount = image_view->level_count;
  image_barrier.subresourceRange.baseArrayLayer = image_view->base_layer;
  image_barrier.subresourceRange.layerCount = image_view->layer_count;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;

  vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);

  VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
  region.bufferOffset = buffer->current_size;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {image->width(), image->height(), 1};

  VkCopyBufferToImageInfo2 buffer_image_info{
      VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
  buffer_image_info.srcBuffer = buffer->vk_handle;
  buffer_image_info.dstImage = image->vk_handle;
  buffer_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  buffer_image_info.regionCount = 1;
  buffer_image_info.pRegions = &region;
  vkCmdCopyBufferToImage2(vk_command_buffer, &buffer_image_info);

  // Transition image to shader read only
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  image_barrier.dstAccessMask = VK_ACCESS_2_NONE;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier2(vk_command_buffer, &dependency_info);

  buffer->current_size += size;
}

void VkStagingBuffer::copy_data(const void *p_data, size_t size,
                                size_t alignment) {
  VulkanBuffer *buffer = p_resource_manager->access_buffer(buffer_handle);
  // Align Up
  buffer->current_size =
      (buffer->current_size + alignment - 1) & ~(alignment - 1);

  if ((buffer->current_size + size) > buffer->vk_device_size) {
    if (size > buffer->vk_device_size) {
      throw std::out_of_range("VkStagingBuffer::Stage - Data size (" +
                              std::to_string(size) +
                              ") exceeds buffer buffer->vk_device_size (" +
                              std::to_string(buffer->vk_device_size) + ")");
    } else {
      flush();
    }
  }

  begin();

  std::memcpy(static_cast<u8 *>(buffer->p_data) + buffer->current_size, p_data,
              size);

  buffer->current_size += size;
}

void VkStagingBuffer::flush() {
  end();

  VkCommandBufferSubmitInfo command_submit_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  command_submit_info.commandBuffer = vk_command_buffer;

  VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_submit_info;

  vkQueueSubmit2(vk_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk_queue);

  VulkanBuffer *buffer = p_resource_manager->access_buffer(buffer_handle);
  buffer->current_size = 0;
}
} // namespace hlx
