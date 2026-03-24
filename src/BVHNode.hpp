#pragma once
#include "Material.hpp"
#include "Triangle.hpp"
// Vendor
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace hlx {
struct alignas(16) BVHNode {
  glm::vec3 aabb_min;
  u32 left_first; // Points to the left node if tri_count is 0, else first
                  // id in a tri_id array
  glm::vec3 aabb_max;
  u32 tri_count;
};

struct alignas(16) BLAS {
public:
  void build(std::span<BVHNode> bvh_nodes, u32 bvh_nodes_offset,
             std::span<TriangleGeom> tris, std::span<glm::vec3> centroids,
             std::span<u32> tri_ids, u32 tri_count, u32 tri_id_offset);
  void refit(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
             std::span<u32> tri_ids);

public:
  u32 bvh_node_idx = 0;
  u32 nodes_count = 0;
  u32 tri_count = 0;
  u32 padding;

private:
  void update_node_bounds(std::span<BVHNode> bvh_nodes,
                          std::span<TriangleGeom> tris, std::span<u32> tri_ids,
                          u32 node_idx);
  void subdivide(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
                 std::span<glm::vec3> centroids, std::span<u32> tri_ids,
                 u32 node_idx);
};

struct alignas(16) BLASInstance {
public:
  void set_transform(const glm::mat4 &transform);

public:
  glm::mat4 inv_transform = glm::mat4(1.f);
  MaterialHandle material_handle;
  u32 blas_id;
  u32 padding;
};
} // namespace hlx
