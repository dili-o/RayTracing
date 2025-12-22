#pragma once

#include "Triangle.hpp"

struct alignas(16) BVHNode {
  Vec3 aabb_min; 
  u32 left_first; // Points to the left node or the first prim

	Vec3 aabb_max;
  u32 prim_count;
  bool is_leaf() const { return prim_count > 0; }
};

void build_bvh_gpu(std::vector<BVHNode> &bvh_nodes, const std::vector<TriangleGPU> &triangles,
									 std::vector<u32> &tri_ids, std::vector<Vec3> tri_centroids, u32 &bvh_depth);
void build_bvh_cpu(std::vector<BVHNode> &bvh_nodes, const std::vector<Triangle> &triangles,
									 std::vector<u32> &tri_ids, std::vector<Vec3> tri_centroids, u32 &bvh_depth);
