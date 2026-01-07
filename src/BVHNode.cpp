#include "BVHNode.hpp"

#define BINS 100

struct Bin {
  AABB bounds;
  i32 prim_count = 0;
};

template <typename Tri> struct TrigTraits;

template <> struct TrigTraits<Triangle> {
  static inline const Vec3 &v0(const Triangle &t) { return t.v0; }
  static inline const Vec3 &v1(const Triangle &t) { return t.v1; }
  static inline const Vec3 &v2(const Triangle &t) { return t.v2; }
};

template <> struct TrigTraits<TriangleGPU> {
  static inline const Vec3 &v0(const TriangleGPU &t) { return t.v0; }
  static inline const Vec3 &v1(const TriangleGPU &t) { return t.v1; }
  static inline const Vec3 &v2(const TriangleGPU &t) { return t.v2; }
};

template <typename TrigType>
f32 evaluate_sah(const BVHNode &node, const TrigType *triangles,
                 const u32 *tri_ids, const Vec3 *tri_centroids, i32 axis,
                 f32 pos) {
  // determine triangle counts and bounds for this split candidate
  AABB left_box, right_box;
  int left_count = 0, right_count = 0;
  for (u32 i = 0; i < node.prim_count; ++i) {
    const TrigType &triangle = triangles[tri_ids[node.left_first + i]];
    const Vec3 &centroid = tri_centroids[tri_ids[node.left_first + i]];
    if (centroid[axis] < pos) {
      left_count++;
      left_box.grow(TrigTraits<TrigType>::v0(triangle));
      left_box.grow(TrigTraits<TrigType>::v1(triangle));
      left_box.grow(TrigTraits<TrigType>::v2(triangle));
    } else {
      right_count++;
      right_box.grow(TrigTraits<TrigType>::v0(triangle));
      right_box.grow(TrigTraits<TrigType>::v1(triangle));
      right_box.grow(TrigTraits<TrigType>::v2(triangle));
    }
  }
  f32 cost =
      left_count * left_box.half_area() + right_count * right_box.half_area();
  return cost > 0.f ? cost : infinity;
}

template <typename TrigType>
static void update_node_bounds(BVHNode *bvh_nodes, const TrigType *triangles,
                               u32 *tri_ids, u32 node_idx) {
  BVHNode &node = bvh_nodes[node_idx];
  node.aabb_min = Vec3(infinity);
  node.aabb_max = Vec3(-infinity);
  for (u32 i = 0; i < node.prim_count; ++i) {
    u32 leaf_tri_id = tri_ids[node.left_first + i];
    const TrigType &leaf_tri = triangles[leaf_tri_id];
    node.aabb_min =
        Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v0(leaf_tri));
    node.aabb_min =
        Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v1(leaf_tri));
    node.aabb_min =
        Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v2(leaf_tri));
    node.aabb_max =
        Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v0(leaf_tri));
    node.aabb_max =
        Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v1(leaf_tri));
    node.aabb_max =
        Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v2(leaf_tri));
  }
}

template <typename TrigType>
static void find_best_plane(const BVHNode &node, const TrigType *triangles,
                            const u32 *tri_ids, const Vec3 *tri_centroids,
                            f32 &out_cost, i32 &out_axis, f32 &out_spilt_pos) {
  for (i32 axis = 0; axis < 3; axis++) {
    if (node.aabb_max[axis] == node.aabb_min[axis])
      continue;
    f32 bounds_min = infinity, bounds_max = -infinity;
    for (int i = 0; i < node.prim_count; ++i) {
      bounds_min = std::min(bounds_min,
                            tri_centroids[tri_ids[node.left_first + i]][axis]);
      bounds_max = std::max(bounds_max,
                            tri_centroids[tri_ids[node.left_first + i]][axis]);
    }
    // Populate bins
    Bin bins[BINS];
    f32 scale = BINS / (bounds_max - bounds_min);
    for (u32 i = 0; i < node.prim_count; ++i) {
      const TrigType &trig = triangles[tri_ids[node.left_first + i]];
      f32 centroid_a = tri_centroids[tri_ids[node.left_first + i]][axis];
      i32 bin_idx =
          (i32)std::min(BINS - 1.f, (centroid_a - bounds_min) * scale);
      bins[bin_idx].prim_count++;
      bins[bin_idx].bounds.grow(TrigTraits<TrigType>::v0(trig));
      bins[bin_idx].bounds.grow(TrigTraits<TrigType>::v1(trig));
      bins[bin_idx].bounds.grow(TrigTraits<TrigType>::v2(trig));
    }

    f32 left_area[BINS - 1], right_area[BINS - 1];
    i32 left_count[BINS - 1], right_count[BINS - 1];
    AABB left_box, right_box;
    i32 left_sum = 0, right_sum = 0;
    for (i32 i = 0; i < BINS - 1; ++i) {
      left_sum += bins[i].prim_count;
      left_count[i] = left_sum;
      left_box.grow(bins[i].bounds);
      left_area[i] = left_box.half_area();

      right_sum += bins[BINS - 1 - i].prim_count;
      right_count[BINS - 2 - i] = right_sum;
      right_box.grow(bins[BINS - 1 - i].bounds);
      right_area[BINS - 2 - i] = right_box.half_area();
    }
    scale = (bounds_max - bounds_min) / BINS;
    for (i32 i = 0; i < BINS - 1; ++i) {
      f32 plane_cost =
          left_count[i] * left_area[i] + right_count[i] * right_area[i];
      if (plane_cost < out_cost)
        out_axis = axis, out_spilt_pos = bounds_min + scale * (i + 1),
        out_cost = plane_cost;
    }
  }
}

template <typename TrigType>
static void subdivide_node(BVHNode *bvh_nodes, const TrigType *triangles,
                           u32 *tri_ids, const Vec3 *tri_centroids,
                           u32 &nodes_used, u32 node_idx, u32 current_depth,
                           u32 &max_depth) {
  max_depth = std::max(max_depth, current_depth);
  BVHNode &node = bvh_nodes[node_idx];
  i32 axis;
  f32 split_pos;
  f32 best_cost = infinity;
  find_best_plane<TrigType>(node, triangles, tri_ids, tri_centroids, best_cost,
                            axis, split_pos);
  Vec3 e = node.aabb_max - node.aabb_min;
  f32 parent_area = e.x * e.y + e.y * e.z + e.z * e.x;
  f32 parent_cost = node.prim_count * parent_area;
  if (best_cost >= parent_cost)
    return;

  // Partition triangles
  i32 i = node.left_first;
  i32 j = i + node.prim_count - 1;
  while (i <= j) {
    if (tri_centroids[tri_ids[i]][axis] < split_pos) {
      i++;
    } else {
      std::swap(tri_ids[i], tri_ids[j--]);
    }
  }
  // Abort split if one of the sides is empty
  i32 left_count = i - node.left_first;
  if (left_count == 0 || left_count == node.prim_count)
    return;
  // Create child nodes
  int left_child_idx = nodes_used++;
  int right_child_idx = nodes_used++;
  bvh_nodes[left_child_idx].left_first = node.left_first;
  bvh_nodes[left_child_idx].prim_count = left_count;
  bvh_nodes[right_child_idx].left_first = i;
  bvh_nodes[right_child_idx].prim_count = node.prim_count - left_count;
  node.left_first = left_child_idx;
  node.prim_count = 0;
  update_node_bounds<TrigType>(bvh_nodes, triangles, tri_ids, left_child_idx);
  update_node_bounds<TrigType>(bvh_nodes, triangles, tri_ids, right_child_idx);
  // recurse
  subdivide_node<TrigType>(bvh_nodes, triangles, tri_ids, tri_centroids,
                           nodes_used, left_child_idx, current_depth + 1,
                           max_depth);
  subdivide_node<TrigType>(bvh_nodes, triangles, tri_ids, tri_centroids,
                           nodes_used, right_child_idx, current_depth + 1,
                           max_depth);
}

BVH::BVH(const void *triangles, size_t triangles_size, bool is_gpu,
         std::vector<u32> &tri_ids, std::vector<Vec3> tri_centroids,
         u32 &bvh_depth)
    : triangles(triangles), tri_ids(tri_ids.data()), nodes_used(2),
      tri_count(triangles_size), is_gpu(is_gpu) {
  std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();

  const size_t N = triangles_size;
  if (bvh_nodes.size() < N * 2 - 1)
    bvh_nodes.resize(N * 2 - 1);
  HASSERT(N);
  HASSERT(tri_ids.size());
  HASSERT(tri_centroids.size());

  BVHNode &root = bvh_nodes[0];
  root.left_first = 0;
  root.prim_count = N;
  bvh_depth = 1;
  if (is_gpu) {
    update_node_bounds<TriangleGPU>(bvh_nodes.data(),
                                    static_cast<const TriangleGPU *>(triangles),
                                    tri_ids.data(), 0);
    // subdivide recursively
    subdivide_node<TriangleGPU>(
        bvh_nodes.data(), static_cast<const TriangleGPU *>(triangles),
        tri_ids.data(), tri_centroids.data(), nodes_used, 0, 1, bvh_depth);
  } else {
    update_node_bounds<Triangle>(bvh_nodes.data(),
                                 static_cast<const Triangle *>(triangles),
                                 tri_ids.data(), 0);
    // subdivide recursively
    subdivide_node<Triangle>(
        bvh_nodes.data(), static_cast<const Triangle *>(triangles),
        tri_ids.data(), tri_centroids.data(), nodes_used, 0, 1, bvh_depth);
  }

  auto end = std::chrono::high_resolution_clock::now();
  f64 seconds = std::chrono::duration<f64>(end - start).count();
  std::cout << "BVH build time: " << seconds << " seconds\n";

  HASSERT(bvh_depth <= 64);
}

void BVH::refit() {
  for (i32 i = nodes_used - 1; i >= 0; --i) {
    // Skipping index 1 because we leave that empty for cache line reasons
    if (i != 1) {
      BVHNode &node = bvh_nodes[i];
      if (node.is_leaf()) {
        if (is_gpu)
          update_node_bounds<TriangleGPU>(
              bvh_nodes.data(), static_cast<const TriangleGPU *>(triangles),
              tri_ids, i);
        else
          update_node_bounds<Triangle>(bvh_nodes.data(),
                                       static_cast<const Triangle *>(triangles),
                                       tri_ids, i);
      } else {
        BVHNode &left_child = bvh_nodes[node.left_first];
        BVHNode &right_child = bvh_nodes[node.left_first + 1];
        node.aabb_min = Vec3::fmin(left_child.aabb_min, right_child.aabb_min);
        node.aabb_max = Vec3::fmax(left_child.aabb_max, right_child.aabb_max);
      }
    }
  }
}

bool BVH::intersect(const Ray &ray, const Interval &ray_t,
                    HitRecord &rec) const {
  HASSERT(!is_gpu);
  const BVHNode *node = &bvh_nodes[0], *stack[64];
  u32 stackPtr = 0;
  f32 closest_so_far = ray_t.max;
  bool hit = false;
  const Triangle *trigs = static_cast<const Triangle *>(triangles);

  // Transform ray
  Vec3 new_origin = make_vec3(inv_transform * Vec4(ray.origin, 1.f));
  Vec3 new_dir = make_vec3(inv_transform * Vec4(ray.direction, 0.f));
  Ray new_ray = Ray(new_origin, new_dir);

  while (1) {
    if (node->is_leaf()) {
      for (u32 i = 0; i < node->prim_count; ++i)
        if (trigs[tri_ids[node->left_first + i]].hit(
                new_ray, Interval(ray_t.min, closest_so_far), rec)) {
          hit = true;
          rec.tri_id = tri_ids[node->left_first + i];
          closest_so_far = rec.t;
        }
      if (stackPtr == 0)
        break;
      else
        node = stack[--stackPtr];

      continue;
    } else {
      const BVHNode *child1 = &bvh_nodes[node->left_first];
      const BVHNode *child2 = &bvh_nodes[node->left_first + 1];
      f32 dist1 = intersect_aabb(new_ray, child1->aabb_min, child1->aabb_max,
                                 closest_so_far);
      f32 dist2 = intersect_aabb(new_ray, child2->aabb_min, child2->aabb_max,
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

  // Transform hit results back to original space
  if (hit) {
    Mat4 transform = inv_transform.inverse();
    Mat4 inv_transform_t = inv_transform.transpose();
    rec.p = make_vec3(transform * Vec4(rec.p, 1.f));
    rec.normal = unit_vector(make_vec3(inv_transform_t * Vec4(rec.normal, 0.f)));
  }
  return hit;
}

void BVH::set_transform(const Mat4 &transform) {
  inv_transform = transform.inverse();

  const Vec3 &bmin = bvh_nodes[0].aabb_min;
  const Vec3 &bmax = bvh_nodes[0].aabb_max;
  bounds = AABB();
  for (i32 i = 0; i < 8; ++i) {
    Vec4 v = Vec4(i & 1 ? bmax.x : bmin.x, i & 2 ? bmax.y : bmin.y,
                  i & 4 ? bmax.z : bmin.z, 1.f);
    v = transform * v;
    bounds.grow(make_vec3(v));
  }
}
