#pragma once

#include "BVHNode.hpp"

struct alignas(16) TLASNode {
  Vec3 aabb_min;
  u32 left_right; // 2x16 bits
  Vec3 aabb_max;
  u32 blas_idx;

  bool is_leaf() const { return left_right == 0; }
};

class TLAS {
public:
  TLAS() = default;
  TLAS(BVH *bvh_list, u32 N);
  void build();
  bool intersect(const Ray &ray, const Interval &ray_t, HitRecord &rec,
                 const Triangle *triangles, const u32 *tri_ids);

  std::vector<TLASNode> tlas_nodes;
  const BVH *blas_list = nullptr;
  u32 nodes_used;
private:
  i32 find_best_match(i32* list, i32 N, i32 a);
};
