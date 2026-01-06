#pragma once
#include "Interval.slang"
#include "Ray.slang"
#include "HitRecord.slang"
#include "Triangle.slang"

struct BVHNode {
  float3 aabb_min; 
  uint left_first; // Points to the left node or the first prim

	float3 aabb_max;
  uint prim_count;
};

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

struct BVH {
  float4x4 transform;
  float4x4 inv_transform;
  uint bvh_nodes_index; float3 padding;
};

bool intersect_bvh(Ray ray, Interval ray_t, inout HitRecord rec,
									 BVH bvh, BVHNode *nodes, Triangle *triangles, uint *tri_ids) {
	uint node_stack[64];
	node_stack[0] = bvh.bvh_nodes_index;
	uint stack_ptr = 0;
	float closest_so_far = ray_t.max;
	bool hit = false;

	// Transform ray
	float3 new_origin = mul(bvh.inv_transform, float4(ray.origin, 1.f)).xyz;
	float3 new_dir = normalize(mul(bvh.inv_transform, float4(ray.direction, 0.f)).xyz);
  Ray new_ray = Ray(new_origin, new_dir);

	while (true) {
		BVHNode node = nodes[node_stack[stack_ptr]];
		if (node.prim_count > 0) {
			for (uint i = 0; i < node.prim_count; ++i) {
				if (triangle_hit(triangles[tri_ids[node.left_first + i]], new_ray, Interval(ray_t.min, closest_so_far), rec)) {
					hit = true;
					rec.tri_id = tri_ids[node.left_first + i];	
					closest_so_far = rec.t;
				}
			}
			if (stack_ptr == 0) break;
			else --stack_ptr;
		} else {
			BVHNode child1 = nodes[node.left_first];
			BVHNode child2 = nodes[node.left_first + 1];
			uint child1_idx = node.left_first;
			uint child2_idx = node.left_first + 1;
			float dist1 = intersect_aabb(new_ray, child1.aabb_min, child1.aabb_max, closest_so_far);
			float dist2 = intersect_aabb(new_ray, child2.aabb_min, child2.aabb_max, closest_so_far);

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
  if (hit) {
		float4x4 transform = bvh.transform;
		float4x4 inv_transform_t = transpose(bvh.inv_transform);
		rec.p = mul(transform, float4(rec.p, 1.f)).xyz;
		rec.normal = normalize(mul(inv_transform_t, float4(rec.normal, 0.f)).xyz);
  }
	return hit;
}

