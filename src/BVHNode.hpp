#pragma once
#include "Triangle.hpp"
// Vendor
#include <glm/vec3.hpp>

namespace hlx {
struct alignas(16) BVHNode {
  glm::vec3 aabb_min;
  u32 left_first; // Points to the left node if tri_count is 0, else first
                  // tri_id
  glm::vec3 aabb_max;
  u32 tri_count;
};

void update_node_bounds(std::span<BVHNode> bvh_nodes,
                        std::span<TriangleGeom> tris, std::span<u32> tri_ids,
                        u32 node_idx);
void subdivide(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
               std::span<glm::vec3> centroids, std::span<u32> tri_ids,
               u32 node_idx, u32 &nodes_used);
} // namespace hlx
