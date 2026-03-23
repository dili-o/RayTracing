#include "BVHNode.hpp"
#include "AABB.hpp"

namespace hlx {
f32 evaluate_sah(BVHNode &node, std::span<TriangleGeom> tris,
                 std::span<glm::vec3> centroids, std::span<u32> tri_ids,
                 i32 axis, f32 split_pos) {
  AABB left_box, right_box;
  i32 left_count = 0, right_count = 0;
  for (u32 i = 0; i < node.tri_count; ++i) {
    u32 tri_id = tri_ids[node.left_first + i];
    TriangleGeom &triangle = tris[tri_id];
    f32 candidate_pos = centroids[tri_id][axis];
    if (candidate_pos < split_pos) {
      ++left_count;
      left_box.grow(triangle.v0);
      left_box.grow(triangle.v1);
      left_box.grow(triangle.v2);
    } else {
      ++right_count;
      right_box.grow(triangle.v0);
      right_box.grow(triangle.v1);
      right_box.grow(triangle.v2);
    }
  }
  f32 cost =
      left_count * left_box.half_area() + right_count * right_box.half_area();
  return cost > 0.f ? cost : infinity;
}

void update_node_bounds(std::span<BVHNode> bvh_nodes,
                        std::span<TriangleGeom> tris, std::span<u32> tri_ids,
                        u32 node_idx) {
  BVHNode &node = bvh_nodes[node_idx];
  node.aabb_min = glm::vec3(infinity);
  node.aabb_max = glm::vec3(-infinity);
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

  // Detemine the split axis using SAH
  i32 best_axis = -1;
  f32 best_pos = 0.f, best_cost = infinity;
  for (i32 axis = 0; axis < 3; ++axis) {
    for (u32 i = 0; i < node.tri_count; ++i) {
      u32 tri_id = tri_ids[node.left_first + i];
      f32 candidate_pos = centroids[tri_id][axis];
      f32 cost =
          evaluate_sah(node, tris, centroids, tri_ids, axis, candidate_pos);
      if (cost < best_cost) {
        best_cost = cost, best_pos = candidate_pos, best_axis = axis;
      }
    }
  }
  glm::vec3 e = node.aabb_max - node.aabb_min;
  f32 parent_area = e.x * e.y + e.y * e.z + e.z * e.x;
  f32 parent_cost = node.tri_count * parent_area;
  if (best_cost >= parent_cost)
    return;
  i32 axis = best_axis;
  f32 split_pos = best_pos;

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
