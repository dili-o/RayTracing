#pragma once
#include "Assert.hpp"
#define VK_NO_PROTOTYPE
#include <volk.h>

#include <vk_mem_alloc.h>
#define VK_CHECK(call)                                                         \
  do {                                                                         \
    VkResult result_ = call;                                                   \
    HASSERT_MSGS(result_ == VK_SUCCESS, "Error code: {}", (u32)result_);       \
  } while (0)

#ifdef _DEBUG
static VkBool32
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    HERROR(" MessageID: {} {}\nMessage: {}\n", pCallbackData->pMessageIdName,
           pCallbackData->messageIdNumber, pCallbackData->pMessage);
    HLX_DEBUG_BREAK;
  } else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    HWARN(" MessageID: {} {}\nMessage: {}\n", pCallbackData->pMessageIdName,
          pCallbackData->messageIdNumber, pCallbackData->pMessage);
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    HINFO(" MessageID: {} {}\nMessage: {}\n", pCallbackData->pMessageIdName,
          pCallbackData->messageIdNumber, pCallbackData->pMessage);
  }

  return VK_FALSE;
}
#endif // _DEBUG

namespace hlx {
void HLX_API PrintVmaStats(VmaAllocator vmaAllocator, VkBool32);

std::vector<char> HLX_API ReadFile(const std::string &filename);

bool HLX_API CompileShader(const std::string &path,
                           const std::string &shaderName,
                           const std::string &outputName,
                           VkShaderStageFlagBits stage,
                           bool generateDebugSymbols,
                           const std::string &defines);

VkShaderModule HLX_API CreateShaderModule(VkDevice vkDevice,
                                          const std::vector<char> &code);

namespace init {
VkImageCreateInfo HLX_API ImageCreateInfo(VkExtent3D extents, u32 mipCount,
                                          VkFormat format,
                                          VkImageUsageFlags usageFlags);

VkImageViewCreateInfo HLX_API
ImageViewCreateInfo(VkImage image, VkFormat format,
                    VkImageAspectFlags aspectFlags, u32 mipLevels);

VmaAllocationCreateInfo HLX_API VmaAllocationInfo(VmaMemoryUsage usageFlags);

VkPipelineDynamicStateCreateInfo HLX_API PipelineDynamicStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo HLX_API
PipelineInputAssemblyStateCreateInfo();

VkPipelineViewportStateCreateInfo HLX_API PipelineViewportStateCreateInfo();

VkPipelineRasterizationStateCreateInfo HLX_API
PipelineRasterizationStateCreateInfo();

VkPipelineMultisampleStateCreateInfo HLX_API
PipelineMultisampleStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo HLX_API
PipelineDepthStencilStateCreateInfo(bool enabled = false);
} // namespace init

} // namespace hlx
