#include "VkUtils.hpp"

namespace hlx {
void push_debug_label(VkCommandBuffer cmd, std::string_view name) {
#ifdef VULKAN_DEBUG_NAMES
  const std::array<float, 4> color = {1.f, 1.f, 1.f, 1.f};
  push_debug_label(cmd, name, color);
#endif // VULKAN_DEBUG_NAMES
}

void push_debug_label(VkCommandBuffer cmd, std::string_view name,
                      const std::array<float, 4> &color) {
#ifdef VULKAN_DEBUG_NAMES
  const VkDebugUtilsLabelEXT debug_label{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = name.data(),
      .color = {color.at(0), color.at(1), color.at(2), color.at(3)}};
  vkCmdBeginDebugUtilsLabelEXT(cmd, &debug_label);
#endif // VULKAN_DEBUG_NAMES
}

void pop_debug_label(VkCommandBuffer cmd) {
#ifdef VULKAN_DEBUG_NAMES
  vkCmdEndDebugUtilsLabelEXT(cmd);
#endif // VULKAN_DEBUG_NAMES
}
} // namespace hlx
