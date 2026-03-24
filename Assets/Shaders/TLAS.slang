#pragma once

#include "BVHNode.slang"

struct TLASNode {
  float3 aabb_min;
  uint left_right; // 2x16 bit
  float3 aabb_max;
  uint blas_instance_idx;

  bool is_leaf() { return left_right == 0; }
};

bool intersect_tlas(Ray ray, Interval ray_t, inout HitRecord rec,
									  TLASNode *tlas_nodes, BLASInstance *blas_instances,
                    BLAS *blases, BVHNode *bvh_nodes, TriangleGeom *tris, uint *tri_ids) {
	uint node_id_stack[64];
	node_id_stack[0] = 0;
	uint stack_ptr = 0;
	float closest_so_far = ray_t.max;
	bool hit = false;

	while (true) {
		TLASNode node = tlas_nodes[node_id_stack[stack_ptr]];
		if (node.is_leaf()) {
      BLASInstance blas_instance = blas_instances[node.blas_instance_idx];
			if (blas_instance.intersect(ray, Interval(ray_t.min, closest_so_far),
                                  rec, blases, bvh_nodes, tris, tri_ids)) {
				hit = true;
				closest_so_far = rec.t;
        rec.blas_instance_id = node.blas_instance_idx;
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
				if (dist2 != 1e30f) {
          node_id_stack[stack_ptr++] = child2_idx;
        }
        node_id_stack[stack_ptr] = child1_idx;
			}
		}
  }

	return hit;
}
