#include "Material.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkStagingBuffer.h"
// Vendor
#include <algorithm>
#include <glm/vec3.hpp>

namespace hlx {

void MaterialManager::init(u32 max_material_count, u32 sizeof_material,
                           std::string_view buffer_name,
                           VkResourceManager *p_rm) {
  reference_counts.resize(max_material_count);
  std::fill(reference_counts.begin(), reference_counts.end(), 0u);
  index_pool.init(max_material_count);

  // Create buffer
  VmaAllocationCreateInfo vma_alloc_info{
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.size = max_material_count * sizeof_material;
  buffer = p_rm->create_buffer(buffer_name, buffer_info, vma_alloc_info);
}

void MaterialManager::shutdown(VkResourceManager *p_rm) {
  p_rm->queue_destroy({buffer});
  index_pool.shutdown();
}

bool MaterialManager::remove_material(const MaterialHandle &material_handle) {
  u32 &ref_count = reference_counts[material_handle.index];
  ref_count = std::max<uint32_t>(ref_count, 1) - 1;

  if (ref_count == 0 && material_indices.contains(material_handle.index)) {
    index_pool.release(material_handle.index);
    material_indices.erase(material_handle.index);
    return true;
  }
  return false;
}

//
// LambertManager //////////////////////////////////////////////////////////
void LambertManager::init(u32 max_material_count, VkResourceManager *p_rm) {
  VkDeviceManager *p_device = p_rm->p_device;
  MaterialManager::init(max_material_count, sizeof(Lambert),
                        "LambertMaterialsBuffer", p_rm);
  materials.resize(max_material_count);
  lambert_textures.resize(max_material_count);
  image_infos.reserve(max_material_count);
  write_infos.reserve(max_material_count);

  // Create Descriptor Pool
  const VkDescriptorPoolSize bindless_pool_size = {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = max_material_count};
  VkDescriptorPoolCreateInfo descriptor_pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  descriptor_pool_info.maxSets = 1;
  descriptor_pool_info.poolSizeCount = 1;
  descriptor_pool_info.pPoolSizes = &bindless_pool_size;
  descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  VK_CHECK(vkCreateDescriptorPool(p_device->vk_device, &descriptor_pool_info,
                                  nullptr, &vk_descriptor_pool));
  p_device->set_resource_name<VkDescriptorPool>(
      VK_OBJECT_TYPE_DESCRIPTOR_POOL, vk_descriptor_pool,
      "LambertMaterialsDescriptorPool");

  // Create Bindless Set Layout and Set
  VkDescriptorSetLayoutBinding bindless_layout_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = max_material_count,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};

  VkDescriptorBindingFlags layout_flags =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

  VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
  flags_info.bindingCount = 1;
  flags_info.pBindingFlags = &layout_flags;

  VkDescriptorSetLayoutCreateInfo bindless_layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  bindless_layout_info.bindingCount = 1;
  bindless_layout_info.pBindings = &bindless_layout_binding;
  bindless_layout_info.flags =
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  bindless_layout_info.pNext = &flags_info;
  VK_CHECK(vkCreateDescriptorSetLayout(p_device->vk_device,
                                       &bindless_layout_info, nullptr,
                                       &vk_descriptor_set_layout));
  p_device->set_resource_name<VkDescriptorSetLayout>(
      VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, vk_descriptor_set_layout,
      "LambertMaterialsSetLayout");

  // Allocate the bindless set
  VkDescriptorSetAllocateInfo bindless_set_alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = vk_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &vk_descriptor_set_layout};
  VK_CHECK(vkAllocateDescriptorSets(
      p_device->vk_device, &bindless_set_alloc_info, &vk_descriptor_set));
  p_device->set_resource_name<VkDescriptorSet>(
      VK_OBJECT_TYPE_DESCRIPTOR_SET, vk_descriptor_set, "LambertMaterialsSet");

  // Create sampler
  VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.minLod = 0.f;
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  texture_sampler = p_rm->create_sampler("TextureSampler", sampler_info);
}

void LambertManager::shutdown(VkResourceManager *p_rm) {
  vkDeviceWaitIdle(p_rm->p_device->vk_device);
  index_pool.release_all();
  p_rm->queue_destroy({texture_sampler});

  // Destroy remaining textures
  for (u32 index : material_indices) {
    p_rm->queue_destroy({.handle = lambert_textures[index]});
  }
  vkDestroyDescriptorSetLayout(p_rm->p_device->vk_device,
                               vk_descriptor_set_layout, nullptr);
  vkDestroyDescriptorPool(p_rm->p_device->vk_device, vk_descriptor_pool,
                          nullptr);
  MaterialManager::shutdown(p_rm);
}

void LambertManager::update(VkDeviceManager *p_device) {
  if (update_descriptor) {
    vkUpdateDescriptorSets(p_device->vk_device, write_infos.size(),
                           write_infos.data(), 0, nullptr);
    image_infos.clear();
    write_infos.clear();
    update_descriptor = false;
  }
}

MaterialHandle LambertManager::add_material(VkStagingBuffer &staging_buffer,
                                            VkResourceManager *p_rm, i32 width,
                                            i32 height, u8 *pixels) {
  u32 index = index_pool.obtain_new();
  // Create Image
  VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo vma_alloc_info{};
  vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_info.format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = image_info.mipLevels;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  std::string name = "Lambert " + std::to_string(index) + "Image";
  std::string view_name = name + "View";
  lambert_textures[index] = p_rm->create_image_view(view_name, name, image_info,
                                                    vma_alloc_info, view_info);
  materials[index] = {index};
  material_indices.insert(index);
  // Stage pixel data
  staging_buffer.stage(pixels, lambert_textures[index],
                       image_info.extent.width * image_info.extent.height * 4);
  // Stage buffer change
  staging_buffer.stage(&materials[index], buffer, index * sizeof(Lambert),
                       sizeof(Lambert));
  // Descriptor update write
  const VkDescriptorImageInfo descriptor_image_info{
      .sampler = p_rm->access_sampler(texture_sampler)->vk_handle,
      .imageView = p_rm->access_image_view(lambert_textures[index])->vk_handle,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  image_infos.push_back(descriptor_image_info);

  const VkWriteDescriptorSet image_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vk_descriptor_set,
      .dstBinding = 0,
      .dstArrayElement = index,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &image_infos[image_infos.size() - 1]};
  write_infos.push_back(image_write);
  update_descriptor = true;
  return MaterialHandle(index, MaterialType::LAMBERT);
}

void LambertManager::remove_material(const MaterialHandle &material_handle,
                                     VkResourceManager *p_rm) {
  if (MaterialManager::remove_material(material_handle))
    p_rm->queue_destroy({.handle = lambert_textures[material_handle.index],
                         .frame_index = p_rm->p_device->frame_count});
}

//
// MetalManager //////////////////////////////////////////////////////////
void MetalManager::init(u32 max_material_count, VkResourceManager *p_rm) {
  MaterialManager::init(max_material_count, sizeof(Metal),
                        "MetalMaterialsBuffer", p_rm);
  materials.resize(max_material_count);
}

void MetalManager::shutdown(VkResourceManager *p_rm) {
  index_pool.release_all();
  MaterialManager::shutdown(p_rm);
}

MaterialHandle MetalManager::add_material(VkStagingBuffer &staging_buffer,
                                          const glm::vec3 &albedo,
                                          const f32 fuzz) {
  u32 index = index_pool.obtain_new();
  materials[index] = {albedo.x, albedo.y, albedo.z, fuzz};
  material_indices.insert(index);
  // Stage addition
  staging_buffer.stage(&materials[index], buffer, index * sizeof(Metal),
                       sizeof(Metal));
  return MaterialHandle(index, MaterialType::METAL);
}

void MetalManager::remove_material(const MaterialHandle &material_handle) {
  MaterialManager::remove_material(material_handle);
}

//
// DielectricManager //////////////////////////////////////////////////////////
void DielectricManager::init(u32 max_material_count, VkResourceManager *p_rm) {
  MaterialManager::init(max_material_count, sizeof(Dielectric),
                        "DielectricMaterialsBuffer", p_rm);
  materials.resize(max_material_count);
}

void DielectricManager::shutdown(VkResourceManager *p_rm) {
  index_pool.release_all();
  MaterialManager::shutdown(p_rm);
}

MaterialHandle DielectricManager::add_material(VkStagingBuffer &staging_buffer,
                                               const f32 refractive_index) {
  u32 index = index_pool.obtain_new();
  materials[index] = {refractive_index};
  material_indices.insert(index);
  // Stage addition
  staging_buffer.stage(&materials[index], buffer, index * sizeof(Dielectric),
                       sizeof(Dielectric));
  return MaterialHandle(index, MaterialType::DIELECTRIC);
}

void DielectricManager::remove_material(const MaterialHandle &material_handle) {
  MaterialManager::remove_material(material_handle);
}

//
// EmissiveManager //////////////////////////////////////////////////////////
void EmissiveManager::init(u32 max_material_count, VkResourceManager *p_rm) {
  MaterialManager::init(max_material_count, sizeof(Emissive),
                        "EmissiveMaterialsBuffer", p_rm);
  materials.resize(max_material_count);
}

void EmissiveManager::shutdown(VkResourceManager *p_rm) {
  index_pool.release_all();
  MaterialManager::shutdown(p_rm);
}

MaterialHandle EmissiveManager::add_material(VkStagingBuffer &staging_buffer,
                                             const glm::vec3 &intensity) {
  u32 index = index_pool.obtain_new();
  materials[index] = {intensity.x, intensity.y, intensity.z, 1.f};
  material_indices.insert(index);
  // Stage addition
  staging_buffer.stage(&materials[index], buffer, index * sizeof(Emissive),
                       sizeof(Emissive));
  return MaterialHandle(index, MaterialType::EMISSIVE);
}

void EmissiveManager::remove_material(const MaterialHandle &material_handle) {
  MaterialManager::remove_material(material_handle);
}
} // namespace hlx
