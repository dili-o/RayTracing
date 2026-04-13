#pragma once
#include "BVHNode.hpp"

namespace hlx {
struct alignas(16) TLASNode {
  glm::vec3 aabb_min;
  u32 left_right; // 2 x 16
  glm::vec3 aabb_max;
  u32 blas_instance_idx;

  bool is_leaf() { return left_right == 0; }
};

struct TLAS {
public:
  void build(std::span<TLASNode> tlas_nodes,
             const std::span<BLASInstance> blas_instances,
             const std::span<u32> blas_instance_indices,
             const std::span<BLAS> blas, const std::span<BVHNode> bvh_nodes);
  u32 node_count;

private:
  i32 find_best_match(std::span<TLASNode> tlas_nodes, std::span<i32> node_list,
                      i32 N, i32 a);
};
} // namespace hlx
