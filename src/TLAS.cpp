#include "TLAS.hpp"

TLAS::TLAS(BVH* bvh_list, u32 N) {
	tlas_nodes.resize(2 * N);
	blas_list = bvh_list;
}

void TLAS::build() {
	// TODO: Hardcoded for now

	// Assign a TLASNode to each BLAS
	tlas_nodes[2].left_blas = 0;
	tlas_nodes[2].aabb_min = blas_list[0].bounds.min;
	tlas_nodes[2].aabb_max = blas_list[0].bounds.max;
	tlas_nodes[2].is_leaf = true;

	tlas_nodes[3].left_blas = 1;
	tlas_nodes[3].aabb_min = blas_list[1].bounds.min;
	tlas_nodes[3].aabb_max = blas_list[1].bounds.max;
	tlas_nodes[3].is_leaf = true;

	// create a root node over the two leaf nodes
	tlas_nodes[0].left_blas = 2;
	tlas_nodes[0].aabb_min = Vec3::fmin(tlas_nodes[2].aabb_min, tlas_nodes[3].aabb_min);
	tlas_nodes[0].aabb_max = Vec3::fmax(tlas_nodes[2].aabb_max, tlas_nodes[3].aabb_max);
	tlas_nodes[0].is_leaf = false;
}

bool TLAS::intersect(const Ray &ray, const Interval &ray_t,
                    HitRecord &rec) {
  const TLASNode *node = &tlas_nodes[0], *stack[64];
  u32 stackPtr = 0;
  bool hit = false;
  f32 closest_so_far = ray_t.max;

  while (1) {
    if (node->is_leaf) {
      if (blas_list[node->left_blas].intersect(ray, ray_t, rec)) {
        hit = true;
				closest_so_far = rec.t;
      }
      if (stackPtr == 0)
        break;
      else
        node = stack[--stackPtr];

      continue;
    } else {
      const TLASNode *child1 = &tlas_nodes[node->left_blas];
      const TLASNode *child2 = &tlas_nodes[node->left_blas + 1];
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
