#pragma once

#ifdef _DEBUG
#define VULKAN_DEBUG_REPORT
// #define VULKAN_EXTRA_VALIDATION
#endif // _DEBUG
#define VULKAN_DEBUG_NAMES

#include "VkErr.h"
// External
#include "Vendor/vk_mem_alloc.h"
#include <optional>

namespace hlx {
struct QueueFamilyIndices {
  std::optional<u32> graphics_family_index;
  std::optional<u32> transfer_family_index;
  std::optional<u32> compute_family_index;

  bool is_complete() const noexcept {
    return graphics_family_index.has_value() &&
           transfer_family_index.has_value() &&
           compute_family_index.has_value();
  }
};

struct VulkanSwapchain {
  VkSurfaceFormatKHR vk_surface_format{};
  std::array<VkPresentModeKHR, 2> vk_present_modes;
  u32 image_count{0};
  u32 current_image_index{0};
  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
  VkSwapchainKHR vk_handle{VK_NULL_HANDLE};
};

struct VkDeviceManager {

public:
  void init();
  void shutdown();
  void reset();
  void begin_frame();
  void end_frame();
  void present();
  void create_swapchain();
  void destroy_swapchain() noexcept;

  void set_vsync(bool enable);

  VkImage get_current_backbuffer() const noexcept {
    return swapchain.images.at(swapchain.current_image_index);
  }

  VkImageView get_current_backbuffer_view() const noexcept {
    return swapchain.image_views.at(swapchain.current_image_index);
  }

  VkCommandBuffer get_current_cmd_buffer() const noexcept {
    return vk_command_buffers.at(current_frame);
  }

  template <typename T>
  void set_resource_name(VkObjectType type, T handle,
                         std::string_view name) const {
#ifdef VULKAN_DEBUG_NAMES
    VkDebugUtilsObjectNameInfoEXT name_info{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = type;
    name_info.objectHandle = (u64)handle;
    name_info.pObjectName = name.data();
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk_device, &name_info));
#endif // VULKAN_DEBUG_NAMES
  }

  void push_debug_label(std::string_view name) const {
    const std::array<float, 4> color = {1.f, 1.f, 1.f, 1.f};
    push_debug_label(name, color);
  }

  void push_debug_label(std::string_view name,
                        const std::array<float, 4> &color) const;
  void pop_debug_label() const;

public:
  VkInstance vk_instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT vk_debug_utils_messenger{VK_NULL_HANDLE};
  VkPhysicalDevice vk_physical_device{VK_NULL_HANDLE};
  VkPhysicalDeviceProperties vk_physical_device_properties;
  VkDevice vk_device{VK_NULL_HANDLE};
  VkSurfaceKHR vk_surface{VK_NULL_HANDLE};
  VulkanSwapchain swapchain;
  VmaAllocator vma_allocator{VK_NULL_HANDLE};
  QueueFamilyIndices queue_family_indices;
  VkQueue vk_graphics_queue{VK_NULL_HANDLE};
  VkQueue vk_transfer_queue{VK_NULL_HANDLE};
  VkQueue vk_compute_queue{VK_NULL_HANDLE};
  VkDescriptorPool vk_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorPool vk_bindless_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSetLayout vk_bindless_set_layout{VK_NULL_HANDLE};
  VkDescriptorSet vk_bindless_set{VK_NULL_HANDLE};
  VkCommandPool vk_command_pool{VK_NULL_HANDLE};
  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> vk_command_buffers;
  u32 current_frame{0};
  std::array<VkFence, MAX_FRAMES_IN_FLIGHT> frame_in_flight_fences{
      VK_NULL_HANDLE};
  std::vector<VkSemaphore> render_finished_semaphores;
  std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> image_available_semaphores;

  u32 back_buffer_width{1280};
  u32 back_buffer_height{720};
  bool vsync_enabled{true};
  bool vsync_changed{false};
  bool swapchain_maintenance{false};
};
} // namespace hlx
