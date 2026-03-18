#pragma once
#include "Material.hpp"
#include "TLAS.hpp"
#include "Vulkan/VkDeviceManager.h"
#include "Vulkan/VkResourceManager.hpp"
#include "Vulkan/VkStagingBuffer.h"

class Renderer {
public:
  void render(u64 frame_number);
  MaterialHandle add_lambert_material(const Vec3 &albedo);
  MaterialHandle add_lambert_material(const std::string &filename);
  MaterialHandle add_metal_material(const Vec3 &albedo, real fuzziness);
  MaterialHandle add_dielectric_material(real refraction_index);
  void add_triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
                    const Vec3 &n0, const Vec3 &n1, const Vec3 &n2, Vec2 uv_0,
                    Vec2 uv_1, Vec2 uv_2, MaterialHandle mat_handle);
  u32 get_triangle_count();
  void add_mesh(u32 triangles_offset, u32 triangle_count,
                const Mat4 &transform);
  void init(u32 image_width_, real aspect_ratio_, u32 samples_per_pixel_,
            u32 max_depth_, real vfov_deg_);
  void shutdown();

public:
  real aspect_ratio;
  u32 max_depth;
  i32 image_width;
  i32 image_height;
  Point3 center;      // camera center
  Point3 pixel00_loc; // Location of pixel 0, 0
  Vec3 pixel_delta_u; // Offset to pixel to the right
  Vec3 pixel_delta_v; // Offset to pixel below
  u32 samples_per_pixel;
  real pixel_samples_scale;
  real vfov;
  Point3 lookat = Point3(0.f, 0.f, -1.f); // Point camera is looking at
  Vec3 vup = Vec3(0.f, 1.f, 0.f);         // camera-relative "up" direction
  Vec3 u, v, w;                           // camera frame basis vectors

  real defocus_angle = 0; // Variation angle of rays through each pixel
  real focus_dist =
      10; // Distance from camera lookfrom point to plane of perfect focus
  Vec3 defocus_disk_u; // Defocus disk horizontal radius
  Vec3 defocus_disk_v; // Defocus disk vertical radius
  bool show_image;     // Open the image after rendering

  std::vector<u32> tri_ids;

public:
  hlx::VkStagingBuffer staging_buffer;
  bool reset_accumulation{true};
  std::vector<GpuLambert> lambert_mats;
  std::vector<GpuMetal> metal_mats;
  std::vector<GpuDielectric> dielectric_mats;
  std::vector<TriangleGPU> triangles;
  std::vector<Vec3> tri_centroids;
  std::vector<BVH_GPU> bvhs_gpu;
  u32 bvh_nodes_size = 0;
  hlx::VkDeviceManager *p_device{nullptr};
  hlx::VkResourceManager *p_resource_manager{nullptr};
  hlx::ImageViewHandle final_image_view{};
  hlx::BufferHandle image_buffer{};
  hlx::ShaderHandle comp_shader_module;
  hlx::PipelineHandle path_tracing_pipeline_handle{};
  hlx::BufferHandle triangles_buffer{};
  hlx::BufferHandle tri_ids_buffer{};
  hlx::BufferHandle tlas_nodes_buffer{};
  hlx::BufferHandle bvhs_buffer{};
  hlx::BufferHandle bvh_nodes_buffer{};
  hlx::BufferHandle lambert_buffer{};
  hlx::BufferHandle metal_buffer{};
  hlx::BufferHandle dielectric_buffer{};
  hlx::BufferHandle uniform_buffer{};
  VkDescriptorPool vk_descriptor_pool;
  VkDescriptorPool vk_bindless_descriptor_pool;
  hlx::SetLayoutHandle vk_scene_data_set_layout;
  hlx::SetLayoutHandle vk_bindless_texture_set_layout;
  VkDescriptorSet vk_scene_data_set;
  VkDescriptorSet vk_bindless_texture_set;
  hlx::SamplerHandle vk_sampler;
  std::vector<hlx::ImageViewHandle> vk_image_views;

protected:
  void initialize_camera(u32 image_width_, real aspect_ratio_,
                         u32 samples_per_pixel_, u32 max_depth_,
                         real vfov_deg_);

protected:
  std::vector<BVH> bvhs;
  TLAS tlas;
};
