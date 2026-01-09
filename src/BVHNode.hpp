#pragma once

#include "Triangle.hpp"
#include "AABB.hpp"
#include "Mat4.hpp"

struct alignas(16) BVHNode {
  Vec3 aabb_min;
  u32 left_first; // Points to the left node or the first prim

  Vec3 aabb_max;
  u32 prim_count;
  bool is_leaf() const { return prim_count > 0; }
};

class BVH {
public:
  BVH() = default;
  BVH(const void *triangles, size_t triangles_size, u32 triangle_offset, bool is_gpu,
      u32 *tri_ids, const Vec3 *tri_centroids,
      u32 &bvh_depth);
  void refit(const void *triangles, u32 *tri_ids);
  bool intersect(const Ray &ray, const Interval &ray_t,
                 HitRecord &rec, const Triangle *triangles,
                 const u32 *tri_ids) const;
  void set_transform(const Mat4 &transform);

  std::vector<BVHNode> bvh_nodes;
  u32 nodes_used, tri_count, trig_offset;
  bool is_gpu;

  Mat4 inv_transform;
  AABB bounds;
};

struct alignas(16) BVH_GPU {
	Mat4 transform;
	Mat4 inv_transform;
	u32 node_index; u32 trig_offset; f32 padding[2];
};
