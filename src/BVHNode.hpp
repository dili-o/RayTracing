#pragma once

#include "Triangle.hpp"

struct alignas(16) BVHNode {
  Vec3 aabb_min; 
  u32 left_first; // Points to the left node or the first prim

	Vec3 aabb_max;
  u32 prim_count;
  bool is_leaf() const { return prim_count > 0; }
};


