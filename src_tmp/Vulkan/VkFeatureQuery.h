#pragma once

#include "Vendor/volk/volk.h"
#include <concepts>
#include <cstddef>

template <typename T>
concept VulkanFeatureStruct =
    std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T> &&
    requires(T t) {
      requires std::same_as<std::remove_cvref_t<decltype(t.sType)>,
                            VkStructureType>;
      requires std::same_as<std::remove_cvref_t<decltype(t.pNext)>, void *>;
    };

template <typename T> consteval VkStructureType FeatureSType() noexcept;

// Specialization traits for each VkPhysicalDevice<T>Features
template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceVulkan11Features>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
}

template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceVulkan12Features>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
}

template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceVulkan13Features>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
}

template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
}

template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceDepthClipControlFeaturesEXT>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT;
}

template <>
consteval VkStructureType
FeatureSType<VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR>() noexcept {
  return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
}

// Get the offset of the first bool in the Feature struct
template <VulkanFeatureStruct T> consteval size_t FeatureBoolOffset() {
  // sType | pNext | VkBool32...
  return offsetof(T, pNext) + sizeof(void *);
}

// Get the Feature count
template <VulkanFeatureStruct T> consteval size_t FeatureBoolCount() {
  return (sizeof(T) - FeatureBoolOffset<T>()) / sizeof(VkBool32);
}

// Query a single Feature struct
template <VulkanFeatureStruct T>
[[nodiscard]]
T QuerySupportedFeatures(VkPhysicalDevice physicalDevice) noexcept {
  T supported{};
  supported.sType = FeatureSType<T>();
  supported.pNext = nullptr;

  VkPhysicalDeviceFeatures2 features2{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features2.pNext = &supported;

  vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
  return supported;
}

template <VulkanFeatureStruct T>
[[nodiscard]]
bool SupportsRequestedFeatures(const T &requested,
                               const T &supported) noexcept {
  const auto *req = reinterpret_cast<const VkBool32 *>(
      reinterpret_cast<const std::byte *>(&requested) + FeatureBoolOffset<T>());

  const auto *sup = reinterpret_cast<const VkBool32 *>(
      reinterpret_cast<const std::byte *>(&supported) + FeatureBoolOffset<T>());

  constexpr size_t count = FeatureBoolCount<T>();

  for (size_t i = 0; i < count; ++i) {
    if (req[i] == VK_TRUE && sup[i] != VK_TRUE)
      return false;
  }

  return true;
}

template <VulkanFeatureStruct T>
[[nodiscard]]
bool PhysicalDeviceSupports(VkPhysicalDevice physicalDevice,
                            const T &requested) noexcept {
  const T supported = QuerySupportedFeatures<T>(physicalDevice);
  return SupportsRequestedFeatures(requested, supported);
}

template <VulkanFeatureStruct... Ts>
void ChainFeatures(VkPhysicalDeviceFeatures2 &features2,
                   Ts &...features) noexcept {
  void **next = &features2.pNext;

  ((features.sType = FeatureSType<Ts>(), features.pNext = nullptr,
    *next = &features, next = &features.pNext),
   ...);
}
