#include "VkStagingBuffer.hpp"
#include "Core/Assert.hpp"
#include "Core/Log.hpp"
#include "Vulkan/VulkanTypes.hpp"

namespace hlx {

void VkStagingBuffer::Shutdown() {
  if (pCtx == nullptr) {
    return;
  }

  vkDestroyCommandPool(pCtx->vkDevice, vkCommandPool, nullptr);
  vmaDestroyBuffer(pCtx->vmaAllocator, buffer.vkHandle, buffer.vmaAllocation);

  pCtx = nullptr;
  vkCommandPool = VK_NULL_HANDLE;
  vkCommandBuffer = VK_NULL_HANDLE;
  capacity = 0;
  currentSize = 0;
  isRecording = false;
}

void VkStagingBuffer::Begin() {
  if (isRecording)
    return;
  VkCommandBufferBeginInfo beginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(vkCommandBuffer, &beginInfo));
  isRecording = true;
}

void VkStagingBuffer::End() {
  if (!isRecording)
    return;

  VK_CHECK(vkEndCommandBuffer(vkCommandBuffer));

  isRecording = false;
}

void VkStagingBuffer::Initialize(const VkContext *pVkContext, size_t size) {
  HASSERT(pVkContext);
  if (buffer.vkHandle != VK_NULL_HANDLE) {
    HWARN("VkStagingBuffer has already been initialized!");
    return;
  }

  util::CreateVmaBuffer(pVkContext->vmaAllocator, pVkContext->vkDevice, buffer,
                        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU,
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  pCtx = pVkContext;

  VkCommandPoolCreateInfo commandPoolCreateInfo{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.queueFamilyIndex =
      pVkContext->queueFamilyIndices.transferFamilyIndex.value();
  VK_CHECK(vkCreateCommandPool(pCtx->vkDevice, &commandPoolCreateInfo, nullptr,
                               &vkCommandPool));

  VkCommandBufferAllocateInfo cbAllocInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbAllocInfo.commandPool = vkCommandPool;
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAllocInfo.commandBufferCount = 1;
  VK_CHECK(
      vkAllocateCommandBuffers(pCtx->vkDevice, &cbAllocInfo, &vkCommandBuffer));

  capacity = size;
}

void VkStagingBuffer::StageBuffer(const void *pData,
                                  const VulkanBuffer *pDstBuffer,
                                  size_t dstOffset, size_t size) {
  if (size == 0)
    return;

  Begin();

  // Data to transfer cannot fit inside the staging buffer
  if ((currentSize + size) > capacity) {
    const size_t remainingSize = capacity - currentSize;
    StageBuffer(pData, pDstBuffer, dstOffset, remainingSize);
    Flush();

    dstOffset += remainingSize;
    size -= remainingSize;

    const u8 *pRemainingData = static_cast<const u8 *>(pData) + remainingSize;

    StageBuffer(pRemainingData, pDstBuffer, dstOffset, size);
  } else {
    std::memcpy(static_cast<u8 *>(buffer.pMappedData) + currentSize, pData,
                size);

    VkBufferCopy2 region{VK_STRUCTURE_TYPE_BUFFER_COPY_2};
    region.srcOffset = currentSize;
    region.dstOffset = dstOffset;
    region.size = size;

    VkCopyBufferInfo2 bufferInfo{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
    bufferInfo.srcBuffer = buffer.vkHandle;
    bufferInfo.dstBuffer = pDstBuffer->vkHandle;
    bufferInfo.regionCount = 1;
    bufferInfo.pRegions = &region;

    vkCmdCopyBuffer2(vkCommandBuffer, &bufferInfo);

    currentSize += size;
  }
}

void VkStagingBuffer::StageImage(const void *pData, const VulkanImage *pImage,
                                 const size_t size, size_t alignment) {
  Begin();

  // Align Up
  currentSize = (currentSize + alignment - 1) & ~(alignment - 1);

  if ((currentSize + size) > capacity) {
    if (size > capacity) {
      HCRITICAL("VkStagingBuffer::StageImage - Image size ({}) exceeds buffer "
                "capacity ({})",
                size, capacity);
    } else {
      Flush();
      StageImage(pData, pImage, size);
      return;
    }
  }

  std::memcpy(static_cast<u8 *>(buffer.pMappedData) + currentSize, pData, size);

  // Transition image to transfer dst
  VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
  imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.image = pImage->vkHandle;
  imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.baseMipLevel = 0;
  imageBarrier.subresourceRange.levelCount = pImage->mipCount;
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount = 1;

  VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependencyInfo.dependencyFlags = 0;
  dependencyInfo.imageMemoryBarrierCount = 1;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;

  vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

  VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
  region.bufferOffset = currentSize;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {pImage->vkExtents.width, pImage->vkExtents.height, 1};

  VkCopyBufferToImageInfo2 bufferImageInfo{
      VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
  bufferImageInfo.srcBuffer = buffer.vkHandle;
  bufferImageInfo.dstImage = pImage->vkHandle;
  bufferImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  bufferImageInfo.regionCount = 1;
  bufferImageInfo.pRegions = &region;
  vkCmdCopyBufferToImage2(vkCommandBuffer, &bufferImageInfo);

  // Transition image to shader read only
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  imageBarrier.dstAccessMask = VK_ACCESS_2_NONE;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

  currentSize += size;
}

void VkStagingBuffer::CopyData(const void *pData, size_t size,
                               size_t alignment) {
  // Align Up
  currentSize = (currentSize + alignment - 1) & ~(alignment - 1);

  if ((currentSize + size) > capacity) {
    if (size > capacity) {
      throw std::out_of_range(
          "VkStagingBuffer::Stage - Data size (" + std::to_string(size) +
          ") exceeds buffer capacity (" + std::to_string(capacity) + ")");
    } else {
      Flush();
    }
  }

  Begin();

  std::memcpy(static_cast<u8 *>(buffer.pMappedData) + currentSize, pData, size);

  currentSize += size;
}

void VkStagingBuffer::Flush() {
  End();

  VkCommandBufferSubmitInfo commandSubmitInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  commandSubmitInfo.commandBuffer = vkCommandBuffer;

  VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &commandSubmitInfo;

  vkQueueSubmit2(pCtx->vkTransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(pCtx->vkTransferQueue);

  currentSize = 0;
}
} // namespace hlx
