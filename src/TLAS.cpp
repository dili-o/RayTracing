#include "TLAS.hpp"
#include "AABB.hpp"
// Vendor
#include <glm/ext/matrix_transform.hpp>

namespace hlx {

void TLAS::build(std::span<TLASNode> tlas_nodes,
                 const std::span<BLASInstance> blas_instances,
                 const std::span<u32> blas_instance_indices,
                 const std::span<BLAS> blas,
                 const std::span<BVHNode> bvh_nodes) {
  node_count = 1;
  std::vector<i32> node_ids(blas_instance_indices.size());
  i32 node_indices = node_ids.size();
  // Assign a TLASleaf node to each BLAS
  for (u32 i = 0; i < blas_instance_indices.size(); ++i) {
    // Find the bounds (in world space)
    u32 blas_inst_id = blas_instance_indices[i];
    u32 blas_idx = blas_instances[blas_inst_id].blas_id;
    glm::vec3 bmin = bvh_nodes[blas[blas_idx].bvh_nodes_offset].aabb_min,
              bmax = bvh_nodes[blas[blas_idx].bvh_nodes_offset].aabb_max;
    AABB bounds = AABB();
    glm::mat4 transform =
        glm::inverse(blas_instances[blas_inst_id].inv_transform);
    for (int j = 0; j < 8; j++) {
      glm::vec3 corner((j & 1) ? bmax.x : bmin.x, (j & 2) ? bmax.y : bmin.y,
                       (j & 4) ? bmax.z : bmin.z);

      glm::vec3 world_pos = glm::vec3(transform * glm::vec4(corner, 1.0f));
      bounds.grow(world_pos);
    }
    node_ids[i] = node_count;
    tlas_nodes[node_count].aabb_min = bounds.min;
    tlas_nodes[node_count].aabb_max = bounds.max;
    tlas_nodes[node_count].blas_instance_idx = i;
    tlas_nodes[node_count++].left_right = 0; // Leaf
  }

  // Use agglomerative clustering to build the TLAS
  int a = 0, b = find_best_match(tlas_nodes, node_ids, node_indices, a);
  while (node_indices > 1) {
    i32 c = find_best_match(tlas_nodes, node_ids, node_indices, b);
    if (a == c) {
      i32 node_id_a = node_ids[a], node_id_b = node_ids[b];
      TLASNode &node_a = tlas_nodes[node_id_a], &node_b = tlas_nodes[node_id_b];
      TLASNode &new_node = tlas_nodes[node_count];
      new_node.left_right = node_id_a + (node_id_b << 16);
      new_node.aabb_min = glm::min(node_a.aabb_min, node_b.aabb_min);
      new_node.aabb_max = glm::max(node_a.aabb_max, node_b.aabb_max);
      node_ids[a] = node_count++;
      node_ids[b] = node_ids[node_indices - 1];
      b = find_best_match(tlas_nodes, node_ids, --node_indices, a);
    } else {
      a = b, b = c;
    }
  }

  tlas_nodes[0] = tlas_nodes[node_ids[a]];
  node_count--;
}

i32 TLAS::find_best_match(std::span<TLASNode> tlas_nodes,
                          std::span<i32> node_list, i32 N, i32 a) {
  f32 smallest = infinity;
  i32 best_b = -1;
  for (i32 b = 0; b < N; ++b) {
    if (b != a) {
      const glm::vec3 bmax = glm::max(tlas_nodes[node_list[a]].aabb_max,
                                      tlas_nodes[node_list[b]].aabb_max);
      const glm::vec3 bmin = glm::min(tlas_nodes[node_list[a]].aabb_min,
                                      tlas_nodes[node_list[b]].aabb_min);
      const glm::vec3 e = bmax - bmin;
      f32 h_surface_area = e.x * e.y + e.y * e.z + e.z * e.x;
      if (h_surface_area < smallest) {
        smallest = h_surface_area;
        best_b = b;
      }
    }
  }
  return best_b;
}
} // namespace hlx
