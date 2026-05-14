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

#ifdef HELIX_WITH_CUDA
typedef void *BufferPtr;
#else
typedef VkDeviceAddress BufferPtr;
#endif // HELIX_WITH_CUDA

struct alignas(16) UniformData {
  vec3 pixel00_loc;
  u32 padding0;
  vec3 pixel_delta_u;
  u32 padding1;
  vec3 pixel_delta_v;
  u32 padding2;
  vec3 camera_center;
  u32 padding3;

  BufferPtr bvh4_data_buffer;
  BufferPtr triangle_shading_buffer;

  BufferPtr tlas_nodes_buffer;
  BufferPtr blas_buffer;

  BufferPtr blas_instances_buffer;
  BufferPtr lambert_materials_buffer;

  BufferPtr metal_materials_buffer;
  BufferPtr dielectric_materials_buffer;

  BufferPtr emissive_materials_buffer;
  BufferPtr padding;
};

struct PushConstant {
  BufferPtr uniform_data_buffer;
#ifdef HELIX_WITH_CUDA
  BufferPtr albedo_textures_buffer;
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
