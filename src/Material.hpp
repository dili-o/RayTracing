#pragma once

#include "Core/FreeIndexPool.hpp"
#include "Vulkan/VkResources.hpp"
// Vendor
#include <glm/fwd.hpp>

enum MaterialType { LAMBERT, METAL, DIELECTRIC, EMISSIVE, NONE };

struct Lambert {
  u32 index;
};

struct alignas(16) Metal {
  f32 albedo_fuzz[4];
};

struct Dielectric {
  f32 refraction_index;
};

struct Emissive {
  f32 intensity[4];
};

struct MaterialHandle {
  u32 index;
  MaterialType type;
};

namespace hlx {

struct VkResourceManager;
struct VkStagingBuffer;
struct VkDeviceManager;

struct MaterialManager {
public:
  // Tracks how many blas instances are using a blas
  std::vector<u32> reference_counts;
  std::unordered_set<u32> material_indices;
  FreeIndexPool index_pool;
  BufferHandle buffer;
  // std::vector<Metal> metal_materials;
protected:
  void init(u32 max_material_count, u32 sizeof_material,
            std::string_view buffer_name, VkResourceManager *p_rm);
  void shutdown(VkResourceManager *p_rm);
  // Returns true if the material handle was removed and false if it wasn't
  bool remove_material(const MaterialHandle &material_handle);
};

struct LambertManager : public MaterialManager {
public:
  void init(u32 max_material_count, VkResourceManager *p_rm);
  void shutdown(VkResourceManager *p_rm);
  void update(VkDeviceManager *p_device);
  MaterialHandle add_material(VkStagingBuffer &staging_buffer,
                              VkResourceManager *p_rm, i32 width, i32 height,
                              u8 *pixels);
  void remove_material(const MaterialHandle &material_handle,
                       VkResourceManager *p_rm);

public:
  std::vector<Lambert> materials;
  VkDescriptorPool vk_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSetLayout vk_descriptor_set_layout{VK_NULL_HANDLE};
  VkDescriptorSet vk_descriptor_set{VK_NULL_HANDLE};
  std::vector<ImageViewHandle> lambert_textures;
  std::vector<VkDescriptorImageInfo> image_infos;
  std::vector<VkWriteDescriptorSet> write_infos;
  SamplerHandle texture_sampler;
  bool update_descriptor{false};
};

struct MetalManager : public MaterialManager {
public:
  void init(u32 max_material_count, VkResourceManager *p_rm);
  void shutdown(VkResourceManager *p_rm);
  MaterialHandle add_material(VkStagingBuffer &staging_buffer,
                              const glm::vec3 &albedo, const f32 fuzz);
  void remove_material(const MaterialHandle &material_handle);

public:
  std::vector<Metal> materials;
};

struct DielectricManager : public MaterialManager {
public:
  void init(u32 max_material_count, VkResourceManager *p_rm);
  void shutdown(VkResourceManager *p_rm);
  MaterialHandle add_material(VkStagingBuffer &staging_buffer,
                              const f32 refractive_index);
  void remove_material(const MaterialHandle &material_handle);

public:
  std::vector<Dielectric> materials;
};

struct EmissiveManager : public MaterialManager {
public:
  void init(u32 max_material_count, VkResourceManager *p_rm);
  void shutdown(VkResourceManager *p_rm);
  MaterialHandle add_material(VkStagingBuffer &staging_buffer,
                              const glm::vec3 &intensity);
  void remove_material(const MaterialHandle &material_handle);

public:
  std::vector<Emissive> materials;
};
} // namespace hlx
