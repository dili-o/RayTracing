#pragma once
#include "Material.hpp"
// Vendor
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace hlx {
struct alignas(16) BLAS {
public:
  // Offset into global BVH4Data buffer (stored as vec4s)
  u32 bvh4_data_offset{0};
  // Offset into global TriangleShadingData buffer (stored as vec4s)
  u32 tri_surface_data_offset{0};
  u32 padding_0{0};
  u32 padding_1{0};
};

struct alignas(16) BLASInstance {
public:
  void set_transform(const glm::mat4 &transform);

public:
  glm::mat4 transform = glm::mat4(1.f);
  glm::mat4 inv_transform = glm::mat4(1.f);
  MaterialHandle material_handle;
  u32 blas_id;
  u32 padding{0u};
};
} // namespace hlx
