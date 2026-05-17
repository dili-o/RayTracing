#pragma once

#ifdef _DEBUG
#define VULKAN_DEBUG_REPORT
#define VULKAN_EXTRA_VALIDATION
#endif // _DEBUG

#include "VkErr.h"
// External
#include <optional>
#include <vk_mem_alloc.h>
#ifdef HELIX_WITH_CUDA
#include "CUDA/CudaErr.hpp"
#include <cuda_runtime.h>
#endif // HELIX_WITH_CUDA

namespace hlx {

struct VkDeviceManager;

struct FrameContext {
public:
  void init(VkDeviceManager *p_device);
  void shutdown(VkDeviceManager *p_device);

public:
  VkCommandBuffer main_cmd{VK_NULL_HANDLE};
  VkFence in_flight_fence{VK_NULL_HANDLE};
  VkSemaphore image_available_semaphore{VK_NULL_HANDLE};

#ifdef HELIX_WITH_CUDA
  VkCommandBuffer post_sumbit_cmd{VK_NULL_HANDLE};
  VkSemaphore vk_cuda_wait_semaphore; // Vulkan signals → CUDA waits
  VkSemaphore vk_cuda_done_semaphore; // CUDA signals → Vulkan waits
  cudaExternalSemaphore_t cu_wait_sem;
  cudaExternalSemaphore_t cu_done_sem;
  bool pre_cuda_cmd_submitted{false};
#endif // HELIX_WITH_CUDA
};

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
public:
  VkSemaphore &get_current_render_finished_semaphore() {
    return render_finished_semaphores.at(current_image_index);
  }

public:
  VkSurfaceFormatKHR vk_surface_format{};
  std::array<VkPresentModeKHR, 2> vk_present_modes;
  u32 image_count{0};
  u32 current_image_index{0};
  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
  std::vector<VkSemaphore> render_finished_semaphores;
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
#ifdef HELIX_WITH_CUDA
    const FrameContext &ctx = frame_contexts.at(current_frame);
    return ctx.pre_cuda_cmd_submitted ? ctx.post_sumbit_cmd : ctx.main_cmd;
#else
    return frame_contexts.at(current_frame).main_cmd;
#endif // HELIX_WITH_CUDA
  }

  FrameContext &get_current_frame_context() noexcept {
    return frame_contexts.at(current_frame);
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
#ifdef HELIX_WITH_CUDA
  cudaStream_t cu_render_stream;
#endif // HELIX_WITH_CUDA
  VkQueue vk_graphics_queue{VK_NULL_HANDLE};
  VkQueue vk_transfer_queue{VK_NULL_HANDLE};
  VkQueue vk_compute_queue{VK_NULL_HANDLE};
  VkDescriptorPool vk_descriptor_pool{VK_NULL_HANDLE};
  VkCommandPool vk_command_pool{VK_NULL_HANDLE};
  std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frame_contexts;
  u32 current_frame{0};
  u64 frame_count{0};

  u32 back_buffer_width{1280};
  u32 back_buffer_height{720};
  bool vsync_enabled{true};
  bool vsync_changed{false};
  bool swapchain_maintenance{false};
};
} // namespace hlx
