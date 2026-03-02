#pragma once

#include "Vulkan/VulkanTypes.hpp"

class VkDeviceManager;

namespace hlx {
class VkStagingBuffer {

public:
  void Initialize(const VkContext *pVkContext, size_t size);
  void Shutdown();
  void Flush();
  void StageBuffer(const void *pData, const VulkanBuffer *pDstBuffer,
             size_t dstOffset, size_t size);
  // NOTE: This function assumes the texture has been recently made and
  // is in UNDEFINED layout. It also assumes that it is a SAMPLED texture
  void StageImage(const void *pData, const VulkanImage *pImage, size_t size,
             size_t alignment = 1);

  // Just copy data to the buffer
  void CopyData(const void *pData, size_t size, size_t alignment = 1);

  size_t CurrentSize() const noexcept { return currentSize; }

public:
  const VkContext *pCtx{nullptr};
  VulkanBuffer buffer{};
  VkCommandPool vkCommandPool{VK_NULL_HANDLE};
  VkCommandBuffer vkCommandBuffer{VK_NULL_HANDLE};

private:
  void Begin();
  void End();

private:
  size_t currentSize{0};
  size_t capacity{0};
  bool isRecording{false};
};
} // namespace hlx
