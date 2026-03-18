#include "PCH.h"

#include "Core/Exceptions.hpp"
#include "Platform/Platform.hpp"
#include "VkDeviceManager.h"
#include "VkFeatureQuery.h"
// External
#include <format>

// TODO: Enable
// #pragma warning(push)
// #pragma warning(disable: 26472)
// #pragma warning(disable: 26485)

#define MAX_TEXTURES 1000

namespace hlx {
#ifdef VULKAN_DEBUG_REPORT
static VkBool32
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data,
               void *p_user_data) noexcept {
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    HERROR(" MessageID: {} {}\nMessage: {}\n", p_callback_data->pMessageIdName,
           p_callback_data->messageIdNumber, p_callback_data->pMessage);
    HLX_DEBUG_BREAK;
  } else if (message_severity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    HWARN(" MessageID: {} {}\nMessage: {}\n", p_callback_data->pMessageIdName,
          p_callback_data->messageIdNumber, p_callback_data->pMessage);
  } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    HINFO(" MessageID: {} {}\nMessage: {}\n", p_callback_data->pMessageIdName,
          p_callback_data->messageIdNumber, p_callback_data->pMessage);
  }

  return VK_FALSE;
}
#endif // VULKAN_DEBUG_REPORT

static VkPhysicalDevice select_physical_device(
    VkInstance vk_instance,
    const std::vector<cstring> &required_extensions_names,
    VkPhysicalDeviceProperties *p_device_properties,
    QueueFamilyIndices &queue_family_indices, VkSurfaceKHR vk_surface,
    const std::vector<VkFormat> &preferred_formats,
    const VkColorSpaceKHR preferred_color_space, VulkanSwapchain &swapchain) {

  u32 device_count = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr));

  if (device_count == 0) {
    std::printf("Failed to find a device that supports Vulkan!\n");
    return VK_NULL_HANDLE;
  }

  std::vector<VkPhysicalDevice> physical_devices(device_count);

  VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &device_count,
                                      physical_devices.data()));

  for (auto device : physical_devices) {
    vkGetPhysicalDeviceProperties(device, p_device_properties);

    // Check for 4x MSAA support
    const VkSampleCountFlags max_sample_count =
        p_device_properties->limits.framebufferColorSampleCounts &
        p_device_properties->limits.framebufferDepthSampleCounts;

    if (!(max_sample_count & VK_SAMPLE_COUNT_4_BIT))
      continue;

    // TODO: Check feature support
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    // Check extension support
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         nullptr);

    std::vector<VkExtensionProperties> extension_properties(extension_count);

    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         extension_properties.data());

    bool has_all_extensions = true;

    for (cstring extension : required_extensions_names) {
      bool found = false;

      for (const VkExtensionProperties &extension_prop : extension_properties) {
        if (strcmp(extension_prop.extensionName, extension) == 0) {
          found = true;
        }
      }

      if (!found) {
        has_all_extensions = false;
        break;
      }

      has_all_extensions = found;
    }

    if (!has_all_extensions)
      continue;

    // Check queue families
    u32 queue_family_count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families.data());

    for (u32 idx = 0; idx < queue_family_count; ++idx) {

      if (queue_families.at(idx).queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          !queue_family_indices.graphics_family_index.has_value()) {

        queue_family_indices.graphics_family_index = idx;
        continue;
      }

      if (queue_families.at(idx).queueFlags & VK_QUEUE_COMPUTE_BIT &&
          !queue_family_indices.compute_family_index.has_value()) {

        queue_family_indices.compute_family_index = idx;
        continue;
      }

      if (queue_families.at(idx).queueFlags & VK_QUEUE_TRANSFER_BIT) {
        queue_family_indices.transfer_family_index = idx;
        continue;
      }
    }

    // Query swapchain support
    u32 format_count;

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vk_surface, &format_count,
                                         nullptr);

    if (format_count == 0)
      continue;

    std::vector<VkSurfaceFormatKHR> formats(format_count);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vk_surface, &format_count,
                                         formats.data());

    u32 present_mode_count;

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vk_surface,
                                              &present_mode_count, nullptr);

    if (present_mode_count == 0)
      continue;

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, vk_surface, &present_mode_count, present_modes.data());

    // Choose format
    bool format_found = false;

    for (size_t i = 0; i < preferred_formats.size(); ++i) {
      for (u32 j = 0; j < format_count; j++) {

        if (formats.at(j).format == preferred_formats.at(i) &&
            formats.at(j).colorSpace == preferred_color_space) {

          swapchain.vk_surface_format = formats.at(j);
          format_found = true;
          break;
        }
      }

      if (format_found)
        break;
    }

    if (!format_found) {
      std::printf("[WARNING]: Could not find preferred surface format, "
                  "defaulting to first available format\n");

      swapchain.vk_surface_format = formats.at(0);
    }

    // Choose present mode
    bool present_mode_found = false;

    for (auto present_mode : present_modes) {

      if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {

        swapchain.vk_present_modes.at(0) = present_mode;
        present_mode_found = true;
        break;
      }
    }

    if (!present_mode_found) {

      std::printf("[WARNING]: Could not find preferred present mode, "
                  "defaulting to FIFO\n");

      swapchain.vk_present_modes.at(0) = VK_PRESENT_MODE_FIFO_KHR;
    }

    swapchain.vk_present_modes.at(1) = VK_PRESENT_MODE_FIFO_KHR;

    if (queue_family_indices.is_complete()) {

      std::printf("Suitable device found: %s\n",
                  p_device_properties->deviceName);

      return device;
    }
  }

  std::printf("No Suitable device found!\n");
  return VK_NULL_HANDLE;
}

void VkDeviceManager::init() {
  const VkResult res = volkInitialize();
  if (res != VK_SUCCESS) {
    throw Exception("Volk failed to initialize!");
  }

  VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pApplicationName = "RayTracer";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_4;

  std::vector<cstring> required_extensions;
  required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  required_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#ifdef VULKAN_DEBUG_NAMES
  required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // VULKAN_DEBUG_NAMES
  std::vector<cstring> instance_layer_names;

  size_t instance_extensions_found = 0;
  u32 instance_extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                         nullptr);
  std::vector<VkExtensionProperties> instance_extensions;
  instance_extensions.resize(instance_extension_count);

  vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                         instance_extensions.data());

  for (cstring required_extension : required_extensions) {
    for (const auto &extension : instance_extensions) {
      const std::string_view ext_name(extension.extensionName);
      if (ext_name == required_extension) {
        ++instance_extensions_found;
        break;
      }
    }
  }

  HASSERT(instance_extensions_found == required_extensions.size());

  VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
#ifdef VULKAN_DEBUG_REPORT

  instance_layer_names.push_back("VK_LAYER_KHRONOS_validation");

  u32 layer_not_found_index = 0;

  auto check_layer_support = [&instance_layer_names, &layer_not_found_index]() {
    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (u32 i = 0; i < instance_layer_names.size(); ++i) {
      bool layer_found = false;
      const std::string_view layer_name = instance_layer_names.at(i);

      for (const auto &layer_property : available_layers) {
        if (layer_name == layer_property.layerName) {
          layer_found = true;
          break;
        }
      }

      if (!layer_found) {
        layer_not_found_index = i;
        return false;
      }
    }

    return true;
  };

  if (!check_layer_support()) {
    std::string msg =
        std::format("Instance layer {} not found!",
                    instance_layer_names.at(layer_not_found_index));
    throw Exception(msg);
  }

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debug_create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
  debug_create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_create_info.pfnUserCallback = debug_callback;
#if defined(VULKAN_EXTRA_VALIDATION)
  const std::array<VkValidationFeatureEnableEXT, 2> features_requested = {
      // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
      // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
      VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
      // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
  };
  VkValidationFeaturesEXT features = {
      VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  features.pNext = &debug_create_info;
  features.enabledValidationFeatureCount = features_requested.size();
  features.pEnabledValidationFeatures = features_requested.data();
  create_info.pNext = &features;
#else
  create_info.pNext = &debug_create_info;
#endif // VULKAN_EXTRA_VALIDATION
#endif // VULKAN_DEBUG_REPORT
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<u32>(required_extensions.size());
  create_info.ppEnabledExtensionNames = required_extensions.data();
  create_info.enabledLayerCount = static_cast<u32>(instance_layer_names.size());
  create_info.ppEnabledLayerNames = instance_layer_names.data();
  VK_CHECK(vkCreateInstance(&create_info, nullptr, &vk_instance));
  volkLoadInstance(vk_instance);

#ifdef VULKAN_DEBUG_REPORT
  if (vkCreateDebugUtilsMessengerEXT(vk_instance, &debug_create_info, nullptr,
                                     &vk_debug_utils_messenger) != VK_SUCCESS) {
    throw Exception("Failed to set up debug messenger!");
  }
#endif // VULKAN_DEBUG_REPORT
  std::vector<cstring> device_extensions{};
  device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  device_extensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
  device_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
  device_extensions.push_back(VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME);
#ifdef VULKAN_EXTRA_VALIDATION
  // device_extensions.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
#endif // VULKAN_EXTRA_VALIDATION

  // Create surface
  if (!Platform::create_vulkan_surface(this)) {
    HERROR("Failed to create surface!");
    throw Exception("Failed to create surface!");
  }

  // Create Physical Device
  vk_physical_device = select_physical_device(
      vk_instance, device_extensions, &vk_physical_device_properties,
      queue_family_indices, vk_surface,
      {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM},
      VK_COLORSPACE_SRGB_NONLINEAR_KHR, swapchain);

  if (vk_physical_device == VK_NULL_HANDLE) {
    throw Exception("No Suitable physical device found");
  }

  // Create Logical Device
  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  constexpr float queue_priority{1.f};

  VkDeviceQueueCreateInfo graphics_queue_info{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  graphics_queue_info.queueFamilyIndex =
      queue_family_indices.graphics_family_index.value();
  graphics_queue_info.queueCount = 1;
  graphics_queue_info.pQueuePriorities = &queue_priority;

  queue_create_infos.push_back(graphics_queue_info);
  if (queue_family_indices.graphics_family_index !=
      queue_family_indices.compute_family_index) {
    VkDeviceQueueCreateInfo queue_create_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_create_info.queueFamilyIndex =
        queue_family_indices.compute_family_index.value();
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  if (queue_family_indices.graphics_family_index !=
      queue_family_indices.transfer_family_index) {
    VkDeviceQueueCreateInfo queue_create_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_create_info.queueFamilyIndex =
        queue_family_indices.transfer_family_index.value();
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceVulkan11Features features_11{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  features_11.pNext = nullptr;
  features_11.shaderDrawParameters = VK_TRUE;

  VkPhysicalDeviceVulkan12Features features_12{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features_12.pNext = nullptr;
  features_12.shaderFloat16 = VK_TRUE;
  features_12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  features_12.bufferDeviceAddress = VK_TRUE;
  features_12.timelineSemaphore = VK_TRUE;
  features_12.drawIndirectCount = VK_TRUE;
  // Bindless
  features_12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  features_12.runtimeDescriptorArray = VK_TRUE;
  features_12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features_12.descriptorBindingPartiallyBound = VK_TRUE;

  // Enable Dynamic Rendering and Synchronization 2
  VkPhysicalDeviceVulkan13Features features_13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features_13.dynamicRendering = VK_TRUE;
  features_13.synchronization2 = VK_TRUE;
  features_13.maintenance4 = VK_TRUE;

  VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchain_maint1{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
  swapchain_maint1.swapchainMaintenance1 = VK_TRUE;

  VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR compute_derivates{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR};
  compute_derivates.computeDerivativeGroupQuads = VK_TRUE;

  VkPhysicalDeviceFeatures2 device_features2{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  // TODO: Check these
  device_features2.features.samplerAnisotropy = VK_TRUE;
  device_features2.features.depthClamp = VK_TRUE;
  device_features2.features.shaderStorageImageMultisample = VK_TRUE;
  device_features2.features.fillModeNonSolid = VK_TRUE;
  ChainFeatures(device_features2, features_11, features_12, features_13,
                swapchain_maint1, compute_derivates);

  bool supported = true;
  supported &= PhysicalDeviceSupports(vk_physical_device, features_11);
  supported &= PhysicalDeviceSupports(vk_physical_device, features_12);
  supported &= PhysicalDeviceSupports(vk_physical_device, features_13);
  swapchain_maintenance =
      PhysicalDeviceSupports(vk_physical_device, swapchain_maint1);
  // Ensure that both present modes are supported
  swapchain_maintenance &=
      swapchain.vk_present_modes.at(0) == VK_PRESENT_MODE_IMMEDIATE_KHR;
  swapchain_maintenance &=
      swapchain.vk_present_modes.at(1) == VK_PRESENT_MODE_FIFO_KHR;
  supported &= PhysicalDeviceSupports(vk_physical_device, compute_derivates);

  if (!supported) {
    throw Exception("Feature set not supported!");
  }

  VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceCreateInfo.pQueueCreateInfos = queue_create_infos.data();
  deviceCreateInfo.queueCreateInfoCount = queue_create_infos.size();
  deviceCreateInfo.pEnabledFeatures = nullptr;
  deviceCreateInfo.enabledExtensionCount = device_extensions.size();
  deviceCreateInfo.ppEnabledExtensionNames = device_extensions.data();

  deviceCreateInfo.pNext = &device_features2;

  VK_CHECK(vkCreateDevice(vk_physical_device, &deviceCreateInfo, nullptr,
                          &vk_device));
  volkLoadDevice(vk_device);

  set_resource_name<VkInstance>(VK_OBJECT_TYPE_INSTANCE, vk_instance,
                                "VulkanInstance");
  set_resource_name<VkSurfaceKHR>(VK_OBJECT_TYPE_SURFACE_KHR, vk_surface,
                                  "WindowSurface");
  set_resource_name<VkDevice>(VK_OBJECT_TYPE_DEVICE, vk_device, "VulkanDevice");
  set_resource_name<VkPhysicalDevice>(VK_OBJECT_TYPE_PHYSICAL_DEVICE,
                                      vk_physical_device,
                                      vk_physical_device_properties.deviceName);

  // Get queues
  vkGetDeviceQueue(vk_device,
                   queue_family_indices.graphics_family_index.value(), 0,
                   &vk_graphics_queue);
  set_resource_name<VkQueue>(VK_OBJECT_TYPE_QUEUE, vk_graphics_queue,
                             "GraphicsQueue");

  if (queue_family_indices.graphics_family_index !=
      queue_family_indices.compute_family_index) {
    vkGetDeviceQueue(vk_device,
                     queue_family_indices.compute_family_index.value(), 0,
                     &vk_compute_queue);
    set_resource_name<VkQueue>(VK_OBJECT_TYPE_QUEUE, vk_compute_queue,
                               "ComputeQueue");
  } else {
    vk_compute_queue = vk_graphics_queue;
  }

  if (queue_family_indices.graphics_family_index !=
      queue_family_indices.transfer_family_index) {
    vkGetDeviceQueue(vk_device,
                     queue_family_indices.transfer_family_index.value(), 0,
                     &vk_transfer_queue);
    set_resource_name<VkQueue>(VK_OBJECT_TYPE_QUEUE, vk_transfer_queue,
                               "TransferQueue");
  } else {
    vk_transfer_queue = vk_graphics_queue;
  }

  // Create VMA Allocator
  VmaVulkanFunctions vma_vulkan_functions{};
  vma_vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vma_vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  vma_vulkan_functions.vkGetPhysicalDeviceProperties =
      vkGetPhysicalDeviceProperties;
  vma_vulkan_functions.vkGetPhysicalDeviceMemoryProperties =
      vkGetPhysicalDeviceMemoryProperties;
  vma_vulkan_functions.vkAllocateMemory = vkAllocateMemory;
  vma_vulkan_functions.vkFreeMemory = vkFreeMemory;
  vma_vulkan_functions.vkMapMemory = vkMapMemory;
  vma_vulkan_functions.vkUnmapMemory = vkUnmapMemory;
  vma_vulkan_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
  vma_vulkan_functions.vkInvalidateMappedMemoryRanges =
      vkInvalidateMappedMemoryRanges;
  vma_vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
  vma_vulkan_functions.vkBindImageMemory = vkBindImageMemory;
  vma_vulkan_functions.vkGetBufferMemoryRequirements =
      vkGetBufferMemoryRequirements;
  vma_vulkan_functions.vkGetImageMemoryRequirements =
      vkGetImageMemoryRequirements;
  vma_vulkan_functions.vkCreateBuffer = vkCreateBuffer;
  vma_vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
  vma_vulkan_functions.vkCreateImage = vkCreateImage;
  vma_vulkan_functions.vkDestroyImage = vkDestroyImage;
  vma_vulkan_functions.vkCmdCopyBuffer = vkCmdCopyBuffer;
  vma_vulkan_functions.vkGetBufferMemoryRequirements2KHR =
      vkGetBufferMemoryRequirements2KHR;
  vma_vulkan_functions.vkGetImageMemoryRequirements2KHR =
      vkGetImageMemoryRequirements2KHR;
  vma_vulkan_functions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
  vma_vulkan_functions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
  vma_vulkan_functions.vkGetPhysicalDeviceMemoryProperties2KHR =
      vkGetPhysicalDeviceMemoryProperties2KHR;

  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.physicalDevice = vk_physical_device;
  allocator_create_info.device = vk_device;
  allocator_create_info.instance = vk_instance;
  allocator_create_info.pVulkanFunctions = &vma_vulkan_functions;
  allocator_create_info.flags =
      VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
      VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&allocator_create_info, &vma_allocator));

  // Create Descriptor pool
  std::array<VkDescriptorPoolSize, 6> pool_sizes = {
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                           .descriptorCount = 32},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLER,
                           .descriptorCount = 32},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           .descriptorCount = 32},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           .descriptorCount = 32},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           .descriptorCount = 32},
      VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           .descriptorCount = 32},
  };
  VkDescriptorPoolCreateInfo descriptor_pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  descriptor_pool_info.maxSets = 32;
  descriptor_pool_info.poolSizeCount = pool_sizes.size();
  descriptor_pool_info.pPoolSizes = pool_sizes.data();

  VK_CHECK(vkCreateDescriptorPool(vk_device, &descriptor_pool_info, nullptr,
                                  &vk_descriptor_pool));
  set_resource_name<VkDescriptorPool>(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                      vk_descriptor_pool, "MainDescriptorPool");

  // Bindless pool
  const VkDescriptorPoolSize bindless_pool_size = {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = MAX_TEXTURES};
  descriptor_pool_info.maxSets = 1;
  descriptor_pool_info.poolSizeCount = 1;
  descriptor_pool_info.pPoolSizes = &bindless_pool_size;
  descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  VK_CHECK(vkCreateDescriptorPool(vk_device, &descriptor_pool_info, nullptr,
                                  &vk_bindless_descriptor_pool));
  set_resource_name<VkDescriptorPool>(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                      vk_bindless_descriptor_pool,
                                      "BindlessDescriptorPool");

  // Create Bindless Set Layout and Set
  VkDescriptorSetLayoutBinding bindless_layout_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = MAX_TEXTURES,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  VkDescriptorSetLayoutCreateInfo bindless_layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  bindless_layout_info.bindingCount = 1;
  bindless_layout_info.pBindings = &bindless_layout_binding;
  bindless_layout_info.flags =
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &bindless_layout_info,
                                       nullptr, &vk_bindless_set_layout));
  set_resource_name<VkDescriptorSetLayout>(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                           vk_bindless_set_layout,
                                           "BindlessSetLayout");

  // Allocate the bindless set
  VkDescriptorSetAllocateInfo bindless_set_alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = vk_bindless_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &vk_bindless_set_layout};
  VK_CHECK(vkAllocateDescriptorSets(vk_device, &bindless_set_alloc_info,
                                    &vk_bindless_set));

  // Create Command pool and buffers
  VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex =
      queue_family_indices.graphics_family_index.value();

  VK_CHECK(
      vkCreateCommandPool(vk_device, &pool_info, nullptr, &vk_command_pool));
  set_resource_name<VkCommandPool>(VK_OBJECT_TYPE_COMMAND_POOL, vk_command_pool,
                                   "MainCommandPool");

  VkCommandBufferAllocateInfo cb_alloc_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cb_alloc_info.commandPool = vk_command_pool;
  cb_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_alloc_info.commandBufferCount = vk_command_buffers.size();
  VK_CHECK(vkAllocateCommandBuffers(vk_device, &cb_alloc_info,
                                    vk_command_buffers.data()));
  for (size_t i = 0; i < vk_command_buffers.size(); ++i) {
    VkCommandBuffer cmd = vk_command_buffers.at(i);
    std::string name = "CommandBuffer_" + std::to_string(i);
    set_resource_name<VkCommandBuffer>(VK_OBJECT_TYPE_COMMAND_BUFFER, cmd,
                                       name);
  }

  // Create Swapchain
  create_swapchain();

  // Create Semaphores
  VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  render_finished_semaphores.resize(swapchain.image_count);
  i32 index = 0;
  for (auto &semaphore : render_finished_semaphores) {
    VK_CHECK(
        vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &semaphore));
    std::string name = "RenderFinishedSemaphore_" + std::to_string(index++);
    set_resource_name<VkSemaphore>(VK_OBJECT_TYPE_SEMAPHORE, semaphore, name);
  }
  index = 0;
  for (auto &semaphore : image_available_semaphores) {
    VK_CHECK(
        vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &semaphore));
    std::string name = "ImageAvailableSemaphore_" + std::to_string(index++);
    set_resource_name<VkSemaphore>(VK_OBJECT_TYPE_SEMAPHORE, semaphore, name);
  }
  VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  index = 0;
  for (auto &fence : frame_in_flight_fences) {
    VK_CHECK(vkCreateFence(vk_device, &fence_info, nullptr, &fence));
    std::string name = "FrameInFlightFence_" + std::to_string(index++);
    set_resource_name<VkFence>(VK_OBJECT_TYPE_FENCE, fence, name);
  }
}

void VkDeviceManager::create_swapchain() {
  // Get surface capabilities
  VkExtent2D swapchain_extents;
  VkSurfaceCapabilitiesKHR surface_capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface,
                                            &surface_capabilities);

  if (surface_capabilities.currentExtent.width == 0)
    return;

  if (surface_capabilities.currentExtent.width != UINT32_MAX) {
    swapchain_extents = surface_capabilities.currentExtent;
  } else {
    const VkExtent2D extents = {back_buffer_width, back_buffer_height};

    swapchain_extents.width =
        std::clamp(extents.width, surface_capabilities.minImageExtent.width,
                   surface_capabilities.maxImageExtent.width);

    swapchain_extents.height =
        std::clamp(extents.height, surface_capabilities.minImageExtent.height,
                   surface_capabilities.maxImageExtent.height);
  }

  u32 image_count = surface_capabilities.minImageCount + 1;

  if (surface_capabilities.maxImageCount > 0 &&
      image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  swapchain.image_count = image_count;

  const VkSwapchainPresentModesCreateInfoEXT modes_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
      .pNext = nullptr,
      .presentModeCount = 2,
      .pPresentModes = swapchain.vk_present_modes.data()};

  VkSwapchainCreateInfoKHR create_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

  create_info.surface = vk_surface;
  create_info.minImageCount = swapchain.image_count;
  create_info.imageFormat = swapchain.vk_surface_format.format;
  create_info.imageColorSpace = swapchain.vk_surface_format.colorSpace;
  create_info.imageExtent = swapchain_extents;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = surface_capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = swapchain.vk_present_modes.at(vsync_enabled);
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = swapchain.vk_handle;

  if (swapchain_maintenance)
    create_info.pNext = &modes_info;

  VK_CHECK(vkCreateSwapchainKHR(vk_device, &create_info, nullptr,
                                &swapchain.vk_handle));

  swapchain.images.resize(swapchain.image_count);
  swapchain.image_views.resize(swapchain.image_count);

  // Get swapchain images and create image views
  VK_CHECK(vkGetSwapchainImagesKHR(vk_device, swapchain.vk_handle,
                                   &swapchain.image_count,
                                   swapchain.images.data()));

  for (u32 i = 0; i < swapchain.image_count; ++i) {
    VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};

    view_info.image = swapchain.images.at(i);
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = swapchain.vk_surface_format.format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(vk_device, &view_info, nullptr,
                               &swapchain.image_views.at(i)));

    std::string name = "SwapchainImage_" + std::to_string(i);
    set_resource_name<VkImage>(VK_OBJECT_TYPE_IMAGE, swapchain.images.at(i),
                               name.c_str());

    name = "SwapchainImageView_" + std::to_string(i);
    set_resource_name<VkImageView>(VK_OBJECT_TYPE_IMAGE_VIEW,
                                   swapchain.image_views.at(i), name.c_str());
  }

  back_buffer_width = swapchain_extents.width;
  back_buffer_height = swapchain_extents.height;
}

void VkDeviceManager::destroy_swapchain() noexcept {
  for (u32 i = 0; i < swapchain.image_count; ++i) {
    vkDestroyImageView(vk_device, swapchain.image_views.at(i), nullptr);
    swapchain.images.at(i) = VK_NULL_HANDLE;
    swapchain.image_views.at(i) = VK_NULL_HANDLE;
  }

  vkDestroySwapchainKHR(vk_device, swapchain.vk_handle, nullptr);
  swapchain.vk_handle = VK_NULL_HANDLE;
}

void VkDeviceManager::shutdown() {
  vkDeviceWaitIdle(vk_device);

  for (auto &semaphore : render_finished_semaphores) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
  }
  for (auto &semaphore : image_available_semaphores) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
  }
  for (auto &fence : frame_in_flight_fences) {
    vkDestroyFence(vk_device, fence, nullptr);
  }
  destroy_swapchain();

  vkDestroyCommandPool(vk_device, vk_command_pool, nullptr);

  vkDestroyDescriptorSetLayout(vk_device, vk_bindless_set_layout, nullptr);
  vkDestroyDescriptorPool(vk_device, vk_bindless_descriptor_pool, nullptr);
  vkDestroyDescriptorPool(vk_device, vk_descriptor_pool, nullptr);
  vmaDestroyAllocator(vma_allocator);

  vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);

  vkDestroyDevice(vk_device, nullptr);

#ifdef VULKAN_DEBUG_REPORT
  vkDestroyDebugUtilsMessengerEXT(vk_instance, vk_debug_utils_messenger,
                                  nullptr);
#endif
  vkDestroyInstance(vk_instance, nullptr);
}

void VkDeviceManager::set_vsync(bool enable) {
  vsync_changed = enable != vsync_enabled;
  vsync_enabled = enable;
  if (vsync_changed && !swapchain_maintenance) {
    reset();
    vsync_changed = false;
  }
}

void VkDeviceManager::reset() {
  vkDeviceWaitIdle(vk_device);

  destroy_swapchain();
  create_swapchain();
}

void VkDeviceManager::begin_frame() {
  // Wait for the frame's fence
  VK_CHECK(vkWaitForFences(vk_device, 1,
                           &frame_in_flight_fences.at(current_frame), VK_TRUE,
                           UINT64_MAX));
  VK_CHECK(
      vkResetFences(vk_device, 1, &frame_in_flight_fences.at(current_frame)));

  // Acquire the next swapchain image
  const VkResult result =
      vkAcquireNextImageKHR(vk_device, swapchain.vk_handle, UINT64_MAX,
                            image_available_semaphores.at(current_frame),
                            VK_NULL_HANDLE, &swapchain.current_image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    reset();
  }

  // Start the command buffer
  VkCommandBuffer cmd = vk_command_buffers.at(current_frame);
  vkResetCommandBuffer(cmd, 0);
  const VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

  // Image Barrier for Swapchain image
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  image_barrier.image = get_current_backbuffer();
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  vkCmdPipelineBarrier2(cmd, &dependency_info);
}

void VkDeviceManager::end_frame() {
  VkCommandBuffer cmd = vk_command_buffers.at(current_frame);

  // Transition swapchain to present mode
  VkImageMemoryBarrier2 image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.dstStageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO: Bottom of pipe?
  image_barrier.dstAccessMask = VK_ACCESS_2_NONE;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  image_barrier.image = get_current_backbuffer();
  image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;

  VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency_info.dependencyFlags = 0;
  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  vkCmdPipelineBarrier2(cmd, &dependency_info);

  VK_CHECK(vkEndCommandBuffer(cmd));

  // Submit frame
  VkCommandBufferSubmitInfo command_submit_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  command_submit_info.commandBuffer = cmd;

  const VkSemaphoreSubmitInfo wait_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .semaphore = image_available_semaphores.at(current_frame),
      .value = 0,
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .deviceIndex = 0};

  const std::array<VkSemaphoreSubmitInfo, 1> signal_infos{VkSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .semaphore = render_finished_semaphores.at(swapchain.current_image_index),
      .value = 0,
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .deviceIndex = 0}};

  VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_submit_info;
  submit_info.waitSemaphoreInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = &wait_info;
  submit_info.signalSemaphoreInfoCount = signal_infos.size();
  submit_info.pSignalSemaphoreInfos = signal_infos.data();

  // VK_CHECK(vkQueueSubmit2(vk_graphics_queue, 1, &submit_info,
  // frame_in_flight_fences.at(current_frame)));
  VkResult res = vkQueueSubmit2(vk_graphics_queue, 1, &submit_info,
                                frame_in_flight_fences.at(current_frame));
  VK_CHECK(res);
}

void VkDeviceManager::present() {
  const VkPresentModeKHR new_mode =
      swapchain.vk_present_modes.at(vsync_enabled);

  VkSwapchainPresentModeInfoEXT switch_info;

  VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &render_finished_semaphores.at(swapchain.current_image_index);

  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain.vk_handle;
  present_info.pImageIndices = &swapchain.current_image_index;
  present_info.pResults = nullptr;
  if (swapchain_maintenance && vsync_changed) {
    switch_info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT,
                   .pNext = nullptr,
                   .swapchainCount = 1,
                   .pPresentModes = &new_mode};
    present_info.pNext = &switch_info;
  }

  const VkResult result = vkQueuePresentKHR(vk_graphics_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    reset();
  } else {
    VK_CHECK(result);
  }

  current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VkDeviceManager::push_debug_label(
    std::string_view name, const std::array<float, 4> &color) const {
#ifdef VULKAN_DEBUG_NAMES
  const VkDebugUtilsLabelEXT debug_label{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = name.data(),
      .color = {color.at(0), color.at(1), color.at(2), color.at(3)}};
  vkCmdBeginDebugUtilsLabelEXT(get_current_cmd_buffer(), &debug_label);
#endif // VULKAN_DEBUG_NAMES
}

void VkDeviceManager::pop_debug_label() const {
#ifdef VULKAN_DEBUG_NAMES
  vkCmdEndDebugUtilsLabelEXT(get_current_cmd_buffer());
#endif // VULKAN_DEBUG_NAMES
}
} // namespace hlx
// #pragma warning(pop)
