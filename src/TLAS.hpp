#pragma once

#include "BVHNode.hpp"

struct alignas(16) TLASNode {
  Vec3 aabb_min;
  u32 left_blas;
  Vec3 aabb_max;
  u32 is_leaf;
};

class TLAS {
public:
  TLAS() = default;
  TLAS(BVH *bvh_list, u32 N);
  void build();
  bool intersect(const Ray &ray, const Interval &ray_t, HitRecord &rec);

  std::vector<TLASNode> tlas_nodes;
  const BVH *blas_list = nullptr;
};
