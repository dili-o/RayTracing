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

bool intersect_aabb(const Ray ray, const float3 bmin, const float3 bmax, const float t) {
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
	return tmax >= tmin && tmin < t && tmax > 0.f;
}

bool intersect_bvh(Ray ray, uint node_idx, Interval ray_t, inout HitRecord rec,
									 BVHNode *nodes, Triangle *triangles){
	BVHNode node_stack[15];
	int stack_index = 0;
	node_stack[stack_index++] = nodes[0];
	
	float closest_so_far = ray_t.max;

	bool hit = false;
	while(stack_index > 0){
		BVHNode node = node_stack[--stack_index];
		if(intersect_aabb(ray, node.aabb_min, node.aabb_max, ray_t.max)){
			// Leaf node
			if (node.prim_count > 0) {
				for (uint i = 0; i < node.prim_count; ++i) {
					if (triangle_hit(triangles[node.left_first + i], ray, Interval(ray_t.min, closest_so_far), rec)) {
						hit = true;
						closest_so_far = rec.t;
					}
				}
			} else{
				node_stack[stack_index++] = nodes[node.left_first + 1];
				node_stack[stack_index++] = nodes[node.left_first];
			}
		}
	}
	return hit;
}
