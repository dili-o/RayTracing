#pragma once
#include "BVHNode.hpp"
#include "Camera.hpp"
#include "Core/FreeIndexPool.hpp"
#include "Core/TlsfAllocator.hpp"
#include "Material.hpp"
#include "TLAS.hpp"
#include "Triangle.hpp"
#include "Vulkan/VkResources.hpp"
#include "Vulkan/VkStagingBuffer.h"
// Vendor
#include <glm/fwd.hpp>

namespace hlx {
struct VkDeviceManager;
struct VkResourceManager;

struct Renderer {
public:
  void init(VkDeviceManager *p_device, VkResourceManager *p_rm,
            u32 output_image_width, u32 output_image_height);
  void shutdown();
  void resize(u32 output_image_width, u32 output_image_height);
  void render(Camera &camera);
  void create_output_image(u32 width, u32 height);

  MaterialHandle add_lambert_material(const glm::vec3 &albedo);
  MaterialHandle add_metal_material(const glm::vec3 &albedo, const f32 fuzz);
  MaterialHandle add_dielectric_material(const f32 refractive_index);
  MaterialHandle add_emissive_material(const glm::vec3 &intensity);

  u32 add_blas(std::span<glm::vec3> positions, std::span<glm::vec3> normals,
               std::span<u32> indices);
  u32 add_blas_instance(u32 blas_index, const glm::mat4 &transform,
                        const MaterialHandle material);

  void remove_blas(u32 blas_id);
  void remove_blas_instance(u32 blas_instance_id);

public:
  VkDeviceManager *p_device{nullptr};
  VkResourceManager *p_rm{nullptr};
  VkStagingBuffer staging_buffer;
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

  u32 sphere_blas_index{UINT32_MAX};
  u32 cube_blas_index{UINT32_MAX};
  u32 plane_blas_index{UINT32_MAX};

private:
  void load_sphere_data();
  void load_cube_data();
  void load_plane_data();
  void build_tlas();

private:
  struct BLAS_Allocation {
    void *tri_id_allocation;
    void *bvh_nodes_allocation;
  };
  std::vector<Lambert> lambert_materials;
  std::vector<Metal> metal_materials;
  std::vector<Emissive> emissive_materials;
  std::vector<Dielectric> dielectric_materials;

  // NOTE: This is only used for creating bvh_nodes
  glm::vec3 *triangle_centroids_data;

  // CPU-side triangle data uploaded to the gpu
  TriangleGeom *tri_geom_data;
  TriangleShading *tri_surface_data;
  TlsfAllocator tri_id_allocator;

  // CPU-side acceleration structure data uploaded to the gpu
  TlsfAllocator bvh_nodes_allocator;
  std::unordered_map<u32, BLAS_Allocation> blas_allocations_map;
  FreeIndexPool blases_index_pool;
  std::vector<BLAS> blases;
  FreeIndexPool blas_inst_index_pool;
  std::vector<BLASInstance> blas_instances;
  std::vector<TLASNode> tlas_nodes;

  u32 bvh_nodes_size{0};
  TLAS tlas;

  bool rebuild_tlas{false};
};
} // namespace hlx
