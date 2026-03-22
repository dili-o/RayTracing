#include "BVHNode.hpp"
// Vendor

namespace hlx {
void update_node_bounds(std::span<BVHNode> bvh_nodes,
                        std::span<TriangleGeom> tris, std::span<u32> tri_ids,
                        u32 node_idx) {
  BVHNode &node = bvh_nodes[node_idx];
  node.aabb_min = glm::vec3(FLT_MAX);
  node.aabb_max = glm::vec3(-FLT_MAX);
  for (u32 i = 0; i < node.tri_count; ++i) {
    u32 tri_id = tri_ids[node.left_first + i];
    node.aabb_min = glm::min(node.aabb_min, glm::vec3(tris[tri_id].v0));
    node.aabb_min = glm::min(node.aabb_min, glm::vec3(tris[tri_id].v1));
    node.aabb_min = glm::min(node.aabb_min, glm::vec3(tris[tri_id].v2));

    node.aabb_max = glm::max(node.aabb_max, glm::vec3(tris[tri_id].v0));
    node.aabb_max = glm::max(node.aabb_max, glm::vec3(tris[tri_id].v1));
    node.aabb_max = glm::max(node.aabb_max, glm::vec3(tris[tri_id].v2));
  }
}

void subdivide(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
               std::span<glm::vec3> centroids, std::span<u32> tri_ids,
               u32 node_idx, u32 &nodes_used) {
  BVHNode &node = bvh_nodes[node_idx];
  if (node.tri_count <= 2)
    return;

  glm::vec3 extent = node.aabb_max - node.aabb_min;
  // Get the longest axis
  u32 axis = (extent.x < extent.y) ? 1 : 0;
  if (extent[axis] < extent.z)
    axis = 2;
  f32 split_pos = extent[axis] * 0.5f + node.aabb_min[axis];

  // Split triangles
  u32 l = node.left_first;
  u32 r = l + node.tri_count - 1;
  while (l <= r) {
    if (centroids[tri_ids[l]][axis] < split_pos) {
      ++l;
    } else {
      std::swap(tri_ids[l], tri_ids[r--]);
    }
  }
  // abort split if one of the sides is empty
  u32 left_count = l - node.left_first;
  if (left_count == 0 || left_count == node.tri_count)
    return;

  // Create child nodes
  u32 left_idx = nodes_used++;
  u32 right_idx = nodes_used++;
  BVHNode &left = bvh_nodes[left_idx];
  left.left_first = node.left_first;
  left.tri_count = left_count;

  BVHNode &right = bvh_nodes[right_idx];
  right.left_first = l;
  right.tri_count = node.tri_count - left_count;

  node.left_first = left_idx;
  node.tri_count = 0;
  update_node_bounds(bvh_nodes, tris, tri_ids, left_idx);
  update_node_bounds(bvh_nodes, tris, tri_ids, right_idx);

  // Recursively partition nodes
  subdivide(bvh_nodes, tris, centroids, tri_ids, left_idx, nodes_used);
  subdivide(bvh_nodes, tris, centroids, tri_ids, right_idx, nodes_used);
}
} // namespace hlx
