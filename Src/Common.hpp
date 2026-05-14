#pragma once
#include "Core/Defines.hpp"
// Vendor
#include <glm/vec3.hpp>
#include <volk/volk.h>

#ifdef USE_CUDA_MATH
#include <helper_math.h>
typedef float3 vec3;
#else
typedef glm::vec3 vec3;
#endif // USE_CUDA_MATH

struct alignas(16) UniformData {
  vec3 pixel00_loc;
  u32 padding0;
  vec3 pixel_delta_u;
  u32 padding1;
  vec3 pixel_delta_v;
  u32 padding2;
  vec3 camera_center;
  u32 padding3;

  VkDeviceAddress triangle_geom_buffer;
  VkDeviceAddress triangle_shading_buffer;
  VkDeviceAddress tlas_nodes_buffer;
  VkDeviceAddress bvh_nodes_buffer;
  VkDeviceAddress blas_buffer;
  VkDeviceAddress blas_instances_buffer;
  VkDeviceAddress tri_ids_buffer;
  VkDeviceAddress lambert_materials_buffer;
  VkDeviceAddress metal_materials_buffer;
  VkDeviceAddress dielectric_materials_buffer;
  VkDeviceAddress emissive_materials_buffer;
};

struct PushConstant {
  VkDeviceAddress uniform_data_buffer;
#ifdef HELIX_WITH_CUDA
  VkDeviceAddress albedo_textures_buffer;
#endif // HELIX_WITH_CUDA

  u32 image_width;
  u32 image_height;

  u32 triangle_count;
  u32 frame_index;

  f32 pixel_sample_scale;
  f32 recip_sqrt_spp;

  u32 sqrt_spp;
  f32 padding;
};
