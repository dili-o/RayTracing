#pragma once

#include "BVHNode.slang"

struct TLASNode {
  float3 aabb_min;
  uint left_right; // 2x16 bit
  float3 aabb_max;
  uint blas_idx;

  bool is_leaf() { return left_right == 0; }
};

bool intersect_tlas(Ray ray, Interval ray_t, inout HitRecord rec,
									 TLASNode *tlas_nodes, BVH *bvhs, BVHNode *bvh_nodes, Triangle *triangles, uint *tri_ids) {
	uint node_stack[64];
	node_stack[0] = 0;
	uint stack_ptr = 0;
	float closest_so_far = ray_t.max;
	bool hit = false;

	while (true) {
		TLASNode node = tlas_nodes[node_stack[stack_ptr]];
		if (node.is_leaf()) {
			if (intersect_bvh(ray, Interval(ray_t.min, closest_so_far), rec, bvhs[node.blas_idx], bvh_nodes, triangles, tri_ids)) {
				hit = true;
				closest_so_far = rec.t;
				rec.bvh_id = node.blas_idx;
			}
			if (stack_ptr == 0) break;
			else --stack_ptr;
		} else {
      uint child1_idx = node.left_right & 0x0000FFFF;
      uint child2_idx = (node.left_right & 0xFFFF0000) >> 16;
			TLASNode child1 = tlas_nodes[child1_idx];
			TLASNode child2 = tlas_nodes[child2_idx];
			float dist1 = intersect_aabb(ray, child1.aabb_min, child1.aabb_max, closest_so_far);
			float dist2 = intersect_aabb(ray, child2.aabb_min, child2.aabb_max, closest_so_far);

			if (dist1 > dist2) {
				// Swap
				float temp = dist1;
				dist1 = dist2;
				dist2 = temp;
				// Swap
				uint temp_c = child1_idx;
				child1_idx = child2_idx;
				child2_idx = temp_c;
			}

			if (dist1 == 1e30f) {
				if (stack_ptr == 0) break;
				else --stack_ptr;
			} else {
				node_stack[stack_ptr] = child1_idx;
				if (dist2 != 1e30f) node_stack[++stack_ptr] = child2_idx;
			}
		}
  }

	return hit;
}

