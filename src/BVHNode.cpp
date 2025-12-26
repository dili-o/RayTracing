#include "BVHNode.hpp"
#include "AABB.hpp"

#define BINS 100

struct Bin {
	AABB bounds; 
	i32 prim_count = 0;
};

template <typename Tri>
struct TrigTraits;

template <>
struct TrigTraits<Triangle> {
    static inline const Vec3& v0(const Triangle& t) { return t.v0; }
    static inline const Vec3& v1(const Triangle& t) { return t.v1; }
    static inline const Vec3& v2(const Triangle& t) { return t.v2; }
};

template <>
struct TrigTraits<TriangleGPU> {
    static inline const Vec3& v0(const TriangleGPU& t) { return t.v0; }
    static inline const Vec3& v1(const TriangleGPU& t) { return t.v1; }
    static inline const Vec3& v2(const TriangleGPU& t) { return t.v2; }
};

template <typename TrigType>
f32 evaluate_sah(const BVHNode &node, const TrigType *triangles,
	const u32 *tri_ids, const Vec3 *tri_centroids, i32 axis, f32 pos) {
	// determine triangle counts and bounds for this split candidate
	AABB left_box, right_box;
	int left_count = 0, right_count = 0;
	for(u32 i = 0; i < node.prim_count; ++i) {
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
	f32 cost = left_count * left_box.half_area() + right_count * right_box.half_area();
	return cost > 0.f ? cost : infinity;
}

template <typename TrigType>
static void update_node_bounds(BVHNode *bvh_nodes, const TrigType *triangles,
	u32 *tri_ids, u32 node_idx) {
  BVHNode& node = bvh_nodes[node_idx];
	node.aabb_min = Vec3(infinity);
	node.aabb_max = Vec3(-infinity);
	for (u32 i = 0; i < node.prim_count; ++i) {
    u32 leaf_tri_id = tri_ids[node.left_first + i];
		const TrigType &leaf_tri = triangles[leaf_tri_id];
		node.aabb_min = Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v0(leaf_tri));
		node.aabb_min = Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v1(leaf_tri));
		node.aabb_min = Vec3::fmin(node.aabb_min, TrigTraits<TrigType>::v2(leaf_tri));
		node.aabb_max = Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v0(leaf_tri));
		node.aabb_max = Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v1(leaf_tri));
		node.aabb_max = Vec3::fmax(node.aabb_max, TrigTraits<TrigType>::v2(leaf_tri));
	}
}

template <typename TrigType>
static void find_best_plane(const BVHNode &node, const TrigType* triangles,
	const u32* tri_ids, const Vec3* tri_centroids,
	f32 &out_cost, i32 &out_axis, f32 &out_spilt_pos) {
	for (i32 axis = 0; axis < 3; axis++) {
		if (node.aabb_max[axis] == node.aabb_min[axis]) continue;
		f32 bounds_min = infinity, bounds_max = -infinity;
		for (int i = 0; i < node.prim_count; ++i) {
			bounds_min = std::min(bounds_min, tri_centroids[tri_ids[node.left_first + i]][axis]);
			bounds_max = std::max(bounds_max, tri_centroids[tri_ids[node.left_first + i]][axis]);
		}
		// Populate bins
		Bin bins[BINS];
		f32 scale = BINS / (bounds_max - bounds_min);
		for (u32 i = 0; i < node.prim_count; ++i) {
			const TrigType& trig = triangles[tri_ids[node.left_first + i]];
			f32 centroid_a = tri_centroids[tri_ids[node.left_first + i]][axis];
			i32 bin_idx = (i32)std::min(BINS - 1.f, (centroid_a - bounds_min) * scale);
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
			f32 plane_cost = left_count[i] * left_area[i] + right_count[i] * right_area[i];
			if (plane_cost < out_cost)
			out_axis = axis, out_spilt_pos = bounds_min + scale * (i + 1), out_cost = plane_cost;
		}
		//for (u32 i = 0; i < 100; ++i) {
		//	f32 candidate_pos = node.aabb_min[axis] + (i * scale);
		//	f32 cost = evaluate_sah<TrigType>(node, triangles, tri_ids, tri_centroids, axis, candidate_pos);
		//	if (cost < out_cost) 
		//		out_spilt_pos = candidate_pos, out_axis = axis, out_cost = cost;
		//}
	}
}

template <typename TrigType>
static void subdivide_node(BVHNode *bvh_nodes, const TrigType *triangles,
															 u32 *tri_ids, const Vec3 *tri_centroids, u32 node_idx,
															 u32 &nodes_used, u32 current_depth, u32 &max_depth) {
	max_depth = std::max(max_depth, current_depth);
  BVHNode& node = bvh_nodes[node_idx];
	i32 axis;
	f32 split_pos;
	f32 best_cost = infinity;
	find_best_plane<TrigType>(node, triangles, tri_ids, tri_centroids,
														best_cost, axis, split_pos);
  Vec3 e = node.aabb_max - node.aabb_min;
	f32 parent_area = e.x * e.y + e.y * e.z + e.z * e.x;
	f32 parent_cost = node.prim_count * parent_area;
	if (best_cost >= parent_cost) return;

  // Partition triangles
  i32 i = node.left_first;
  i32 j = i + node.prim_count - 1;
  while (i <= j) {
    if (tri_centroids[tri_ids[i]][axis] < split_pos) {
			i++;
    }
    else {
			std::swap(tri_ids[i], tri_ids[j--]);
    }
  }
  // Abort split if one of the sides is empty
	i32 left_count = i - node.left_first;
	if (left_count == 0 || left_count == node.prim_count) return;
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
										 left_child_idx, nodes_used, current_depth + 1, max_depth);
	subdivide_node<TrigType>(bvh_nodes, triangles, tri_ids, tri_centroids,
										 right_child_idx, nodes_used, current_depth + 1, max_depth);
}

void build_bvh_gpu(std::vector<BVHNode> &bvh_nodes, const std::vector<TriangleGPU> &triangles,
									 std::vector<u32> &tri_ids, std::vector<Vec3> tri_centroids, u32 &bvh_depth) {
	std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();

  const size_t N = triangles.size();
  if (bvh_nodes.size() < N * 2 - 1)
		bvh_nodes.resize(N * 2 - 1);
  u32 nodes_used = 1;
	HASSERT(triangles.size());
	HASSERT(tri_ids.size());
	HASSERT(tri_centroids.size());

  BVHNode& root = bvh_nodes[0];
	root.left_first = 0;
	root.prim_count = N;
	update_node_bounds<TriangleGPU>(bvh_nodes.data(), triangles.data(), tri_ids.data(), 0);
	// subdivide recursively
  bvh_depth = 1;
	subdivide_node<TriangleGPU>(bvh_nodes.data(), triangles.data(), tri_ids.data(),
		                 tri_centroids.data(), 0, nodes_used, 1, bvh_depth);

  auto end = std::chrono::high_resolution_clock::now();
  f64 seconds = std::chrono::duration<f64>(end - start).count();
  std::cout << "BVH build time: " << seconds << " seconds\n";

  HASSERT(bvh_depth <= 64);
}

void build_bvh_cpu(std::vector<BVHNode> &bvh_nodes, const std::vector<Triangle> &triangles,
									 std::vector<u32> &tri_ids, std::vector<Vec3> tri_centroids, u32 &bvh_depth) {
	std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();

  const size_t N = triangles.size();
  if (bvh_nodes.size() < N * 2 - 1)
		bvh_nodes.resize(N * 2 - 1);
  u32 nodes_used = 1;
	HASSERT(triangles.size());
	HASSERT(tri_ids.size());
	HASSERT(tri_centroids.size());

  BVHNode& root = bvh_nodes[0];
	root.left_first = 0;
	root.prim_count = N;
	update_node_bounds<Triangle>(bvh_nodes.data(), triangles.data(), tri_ids.data(), 0);
	// subdivide recursively
  bvh_depth = 1;
	subdivide_node<Triangle>(bvh_nodes.data(), triangles.data(), tri_ids.data(),
		                 tri_centroids.data(), 0, nodes_used, 1, bvh_depth);

  auto end = std::chrono::high_resolution_clock::now();
  f64 seconds = std::chrono::duration<f64>(end - start).count();
  std::cout << "BVH build time: " << seconds << " seconds\n";
}

