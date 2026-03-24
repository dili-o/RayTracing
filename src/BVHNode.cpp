#include "BVHNode.hpp"
#include "AABB.hpp"
#include "Triangle.hpp"

constexpr u32 BIN_COUNT = 200;

namespace hlx {

struct Bin {
  AABB bounds;
  u32 tri_count = 0;
};

static f32 find_best_split_plane(BVHNode &node, std::span<TriangleGeom> tris,
                                 std::span<glm::vec3> centroids,
                                 std::span<u32> tri_ids, i32 &axis,
                                 f32 &split_pos) {
  f32 best_cost = infinity;

  for (i32 a = 0; a < 3; ++a) {
    f32 bounds_min = infinity;
    f32 bounds_max = -infinity;
    for (u32 i = 0; i < node.tri_count; ++i) {
      f32 &centroid_pos = centroids[tri_ids[node.left_first + i]][a];
      bounds_min = std::min(bounds_min, centroid_pos);
      bounds_max = std::max(bounds_max, centroid_pos);
    }
    if (bounds_min == bounds_max)
      continue;
    // Populate bins
    Bin bins[BIN_COUNT];
    f32 scale = BIN_COUNT / (bounds_max - bounds_min);
    for (u32 i = 0; i < node.tri_count; ++i) {
      TriangleGeom &triangle = tris[tri_ids[node.left_first + i]];
      f32 &centroid_pos = centroids[tri_ids[node.left_first + i]][a];

      u32 bin_idx =
          std::min(BIN_COUNT - 1, u32((centroid_pos - bounds_min) * scale));
      Bin &bin = bins[bin_idx];
      bin.tri_count++;
      bin.bounds.grow(triangle.v0);
      bin.bounds.grow(triangle.v1);
      bin.bounds.grow(triangle.v2);
    }

    f32 left_half_area[BIN_COUNT - 1], right_half_area[BIN_COUNT - 1];
    i32 left_count[BIN_COUNT - 1], right_count[BIN_COUNT - 1];
    AABB left_box, right_box;
    i32 left_sum = 0, right_sum = 0;
    for (u32 i = 0; i < BIN_COUNT - 1; ++i) {
      left_sum += bins[i].tri_count;
      left_count[i] = left_sum;
      left_box.grow(bins[i].bounds);
      left_half_area[i] = left_box.half_area();

      right_sum += bins[BIN_COUNT - 1 - i].tri_count;
      right_count[BIN_COUNT - 2 - i] = right_sum;
      right_box.grow(bins[BIN_COUNT - 1 - i].bounds);
      right_half_area[BIN_COUNT - 2 - i] = right_box.half_area();
    }

    scale = (bounds_max - bounds_min) / BIN_COUNT;
    for (u32 i = 0; i < BIN_COUNT - 1; ++i) {
      f32 plane_cost = left_count[i] * left_half_area[i] +
                       right_count[i] * right_half_area[i];
      if (plane_cost < best_cost) {
        best_cost = plane_cost, split_pos = bounds_min + scale * (i + 1),
        axis = a;
      }
    }
  }

  return best_cost;
}

void BLAS::build(std::span<BVHNode> bvh_nodes, u32 bvh_nodes_offset,
                 std::span<TriangleGeom> tris, std::span<glm::vec3> centroids,
                 std::span<u32> tri_ids, u32 tri_count, u32 tri_id_offset) {
  this->bvh_node_idx = bvh_nodes_offset;
  this->nodes_count = 1;
  this->tri_count = tri_count;
  BVHNode &root = bvh_nodes[bvh_node_idx];
  root.left_first = tri_id_offset;
  root.tri_count = tri_count;
  update_node_bounds(bvh_nodes, tris, tri_ids, bvh_node_idx);
  subdivide(bvh_nodes, tris, centroids, tri_ids, bvh_node_idx);
}

void BLAS::update_node_bounds(std::span<BVHNode> bvh_nodes,
                              std::span<TriangleGeom> tris,
                              std::span<u32> tri_ids, u32 node_idx) {
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

void BLAS::subdivide(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
                     std::span<glm::vec3> centroids, std::span<u32> tri_ids,
                     u32 node_idx) {
  BVHNode &node = bvh_nodes[node_idx];

  // Detemine the split axis using SAH
  i32 best_axis = -1;
  f32 best_pos = 0.f;
  f32 best_cost = find_best_split_plane(node, tris, centroids, tri_ids,
                                        best_axis, best_pos);

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
  u32 left_idx = nodes_count++;
  u32 right_idx = nodes_count++;
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
  subdivide(bvh_nodes, tris, centroids, tri_ids, left_idx);
  subdivide(bvh_nodes, tris, centroids, tri_ids, right_idx);
}

void BLAS::refit(std::span<BVHNode> bvh_nodes, std::span<TriangleGeom> tris,
                 std::span<u32> tri_ids) {
  for (u32 i = nodes_count + bvh_node_idx - 1; i >= nodes_count; --i) {
    BVHNode &node = bvh_nodes[i];
    // Is leaf?
    if (node.tri_count) {
      update_node_bounds(bvh_nodes, tris, tri_ids, i);
    } else {
      BVHNode &left_child = bvh_nodes[node.left_first];
      BVHNode &right_child = bvh_nodes[node.left_first + 1];
      node.aabb_min = glm::min(left_child.aabb_min, right_child.aabb_min);
      node.aabb_max = glm::max(left_child.aabb_max, right_child.aabb_max);
    }
  }
}

void BLASInstance::set_transform(const glm::mat4 &transform) {
  this->transform = transform;
  this->inv_transform = glm::inverse(transform);
}

} // namespace hlx
