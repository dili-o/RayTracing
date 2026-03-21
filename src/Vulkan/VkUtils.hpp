#pragma once
#include <volk/volk.h>

namespace hlx {
void push_debug_label(VkCommandBuffer cmd, std::string_view name);
void push_debug_label(VkCommandBuffer cmd, std::string_view name,
                      const std::array<float, 4> &color);
void pop_debug_label(VkCommandBuffer cmd);
} // namespace hlx
