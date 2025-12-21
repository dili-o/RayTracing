#pragma once

#include "Triangle.hpp"

struct BVHNode {
  Vec3 aabb_min, aabb_max;
  u32 left_child;
  u32 first_prim, prim_count;
  bool is_leaf() const { return prim_count > 0; }
};


