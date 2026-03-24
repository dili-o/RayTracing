#pragma once
#include "Interval.slang"
#include "Ray.slang"
#include "HitRecord.slang"
#include "Triangle.slang"
#include "Material.slang"

float intersect_aabb(const Ray ray, const float3 bmin, const float3 bmax, const float t) {
	float tx1 = (bmin.x - ray.origin.x) / ray.direction.x;
	float tx2 = (bmax.x - ray.origin.x) / ray.direction.x;
	float tmin = min(tx1, tx2);
	float tmax = max(tx1, tx2);
	float ty1 = (bmin.y - ray.origin.y) / ray.direction.y;
	float ty2 = (bmax.y - ray.origin.y) / ray.direction.y;
	tmin = max(tmin, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));
	float tz1 = (bmin.z - ray.origin.z) / ray.direction.z;
	float tz2 = (bmax.z - ray.origin.z) / ray.direction.z;
	tmin = max(tmin, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));
	if (tmax >= tmin && tmin < t && tmax > 0) return tmin;
	else return 1e30f;
}

struct BVHNode {
  float3 aabb_min; 
  uint left_first; // Points to the left node or the first prim

	float3 aabb_max;
  uint tri_count;
};

struct BLAS {
  uint bvh_node_idx;
  uint nodes_count;
  uint tri_count;
  uint padding;

bool intersect(Ray ray, Interval ray_t, inout HitRecord rec,
                    BVHNode *nodes, TriangleGeom *tris, uint *tri_ids) {
	uint node_id_stack[64];
  // Initialize first item in the stack to the root node
	node_id_stack[0] = bvh_node_idx;
	uint stack_ptr = 0;
	float closest_so_far = ray_t.max;
	bool hit = false;

  while (true) {
		BVHNode node = nodes[node_id_stack[stack_ptr]];
		if (node.tri_count > 0) { // The node is a leaf
			for (uint i = 0; i < node.tri_count; ++i) {
				uint tri_id_index = node.left_first + i;
				if ((tris[tri_ids[tri_id_index]].hit(ray, Interval(ray_t.min, closest_so_far), rec))) {
					hit = true;
					rec.tri_id = tri_ids[tri_id_index];	
					closest_so_far = rec.t;
				}
			}

      // Terminate when stack is empty
			if (stack_ptr == 0) break;
			else --stack_ptr;
    } else { // The node has children
			uint child1_idx = node.left_first;
			uint child2_idx = child1_idx + 1;
			BVHNode child1 = nodes[child1_idx];
			BVHNode child2 = nodes[child2_idx];
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

			if (dist1 == 1e30f) { // None of the aabbs were hit
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
};

struct BLASInstance {
  float4x4 transform;
  float4x4 inv_transform;

  MaterialHandle material_handle;
  uint blas_index;
  uint padding;

bool intersect(Ray ray, Interval ray_t, inout HitRecord rec,
               BLAS *blases, BVHNode *bvh_nodes, TriangleGeom *tris, uint *tri_ids) {
  Ray world_ray = ray;

	ray.origin = mul(inv_transform, float4(ray.origin, 1.f)).xyz;
	ray.direction = mul(inv_transform, float4(ray.direction, 0.f)).xyz;
  return blases[blas_index].intersect(ray, ray_t, rec, bvh_nodes, tris, tri_ids);
}
};
