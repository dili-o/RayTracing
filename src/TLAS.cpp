#include "TLAS.hpp"

TLAS::TLAS(BVH *bvh_list, u32 N) {
  tlas_nodes.resize(2 * N);
  blas_list = bvh_list;
}

void TLAS::build() {
  const size_t blas_count = tlas_nodes.size() / 2;
  std::vector<i32> tlas_node_ids(blas_count);
  i32 node_indices = blas_count;

  nodes_used = 1;
  for (size_t i = 0; i < blas_count; ++i) {
    tlas_node_ids[i] = nodes_used;
    tlas_nodes[nodes_used].left_right = 0; // Set a leaf node
    tlas_nodes[nodes_used].aabb_min = blas_list[i].bounds.min;
    tlas_nodes[nodes_used].aabb_max = blas_list[i].bounds.max;
    tlas_nodes[nodes_used++].blas_idx = i;
  }

  // Agglomerative clustering to build the TLAS
  i32 a = 0, b = find_best_match(tlas_node_ids.data(), node_indices, a);
  while (node_indices > 1) {
    i32 c = find_best_match(tlas_node_ids.data(), node_indices, b);
    if (a == c) {
      i32 node_idx_a = tlas_node_ids[a];
      i32 node_idx_b = tlas_node_ids[b];
      const TLASNode &node_a = tlas_nodes[node_idx_a];
      const TLASNode &node_b = tlas_nodes[node_idx_b];
      TLASNode &new_node = tlas_nodes[nodes_used];
      new_node.left_right = node_idx_a + (node_idx_b << 16);
      new_node.aabb_min = Vec3::fmin(node_a.aabb_min, node_b.aabb_min);
      new_node.aabb_max = Vec3::fmax(node_a.aabb_max, node_b.aabb_max);
      tlas_node_ids[a] = nodes_used++;
      tlas_node_ids[b] = tlas_node_ids[node_indices - 1];
      b = find_best_match(tlas_node_ids.data(), --node_indices, a);
    } else {
      a = b, b = c;
    }
  }
  tlas_nodes[0] = tlas_nodes[tlas_node_ids[a]];
} 

bool TLAS::intersect(const Ray &ray, const Interval &ray_t, HitRecord &rec,
										 const Triangle *triangles, const u32 *tri_ids) {
  const TLASNode *node = &tlas_nodes[0], *stack[64];
  u32 stackPtr = 0;
  bool hit = false;
  f32 closest_so_far = ray_t.max;

  while (1) {
    if (node->is_leaf()) {
      if (blas_list[node->blas_idx].intersect(ray, ray_t, rec, triangles, tri_ids)) {
        hit = true;
        closest_so_far = rec.t;
      }
      if (stackPtr == 0)
        break;
      else
        node = stack[--stackPtr];

      continue;
    } else {
      const u32 l_idx = node->left_right & 0x0000FFFF;
      const u32 r_idx = (node->left_right & 0xFFFF0000) >> 16;
      const TLASNode *child1 = &tlas_nodes[l_idx];
      const TLASNode *child2 = &tlas_nodes[r_idx];
      f32 dist1 = intersect_aabb(ray, child1->aabb_min, child1->aabb_max,
                                 closest_so_far);
      f32 dist2 = intersect_aabb(ray, child2->aabb_min, child2->aabb_max,
                                 closest_so_far);
      if (dist1 > dist2) {
        std::swap(dist1, dist2);
        std::swap(child1, child2);
      }
      if (dist1 == infinity) {
        if (stackPtr == 0)
          break;
        else
          node = stack[--stackPtr];
      } else {
        node = child1;
        if (dist2 != infinity)
          stack[stackPtr++] = child2;
      }
    }
  }

  return hit;
}

i32 TLAS::find_best_match(i32 *list, i32 N, i32 a) {
  f32 smallest = infinity;
  i32 best_b = -1;
  for (i32 b = 0; b < N; ++b) if(b != a) {
    const Vec3 bmax =
        Vec3::fmax(tlas_nodes[list[a]].aabb_max, tlas_nodes[list[b]].aabb_max);
    const Vec3 bmin =
        Vec3::fmin(tlas_nodes[list[a]].aabb_min, tlas_nodes[list[b]].aabb_min);
    const Vec3 e = bmax - bmin;
    f32 h_surface_area = e.x * e.y + e.y * e.z + e.z * e.x;
    if (h_surface_area < smallest) {
      smallest = h_surface_area;
      best_b = b;
    }
  }
  return best_b;
}
