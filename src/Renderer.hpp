#pragma once
#include "Camera.hpp"
#include "Material.hpp"
#include "Vulkan/VkResources.hpp"
// Vendor
#include <glm/fwd.hpp>

namespace hlx {
struct VkDeviceManager;
struct VkResourceManager;
struct VkStagingBuffer;

struct Renderer {
public:
  void init(VkDeviceManager *p_device, VkResourceManager *p_rm,
            VkStagingBuffer &staging_buffer, u32 output_image_width,
            u32 output_image_height);
  void shutdown();
  void resize(u32 output_image_width, u32 output_image_height);
  void render(Camera &camera);
  void create_output_image(u32 width, u32 height);

  MaterialHandle add_lambert_material(const glm::vec3 &albedo);
  MaterialHandle add_metal_material(const glm::vec3 &albedo, const f32 fuzz);
  MaterialHandle add_dielectric_material(const f32 refractive_index);
  MaterialHandle add_emissive_material(const glm::vec3 &intensity);

  void add_sphere_(f32 radius, const glm::vec3 &center, MaterialHandle mat);
  void add_plane_(f32 width, f32 depth, const glm::vec3 &center,
                  MaterialHandle mat);

public:
  VkDeviceManager *p_device{nullptr};
  VkResourceManager *p_rm{nullptr};
  VkStagingBuffer *p_staging_buffer{nullptr};
  ImageViewHandle output_image_view;
  PipelineHandle path_tracing_pipeline;
  SetLayoutHandle set_layout;
  VkDescriptorSet vk_set;
  std::array<BufferHandle, MAX_FRAMES_IN_FLIGHT> uniform_buffers;
  BufferHandle triangle_geom_buffer;
  BufferHandle triangle_shading_buffer;
  BufferHandle tlas_nodes_buffer;
  BufferHandle bvh_nodes_buffer;
  BufferHandle blas_buffer;
  BufferHandle blas_instances_buffer;
  BufferHandle tri_ids_buffer;
  BufferHandle lambert_materials_buffer;
  BufferHandle metal_materials_buffer;
  BufferHandle dielectric_materials_buffer;
  BufferHandle emissive_materials_buffer;
  u32 total_triangle_count{0};
  u32 frame_index{0};

private:
  void load_sphere_data();
  void load_cube_data();
  void load_plane_data();

private:
  std::vector<Lambert> lambert_materials;
  std::vector<Metal> metal_materials;
  std::vector<Emissive> emissive_materials;
  std::vector<Dielectric> dielectric_materials;
};
} // namespace hlx
