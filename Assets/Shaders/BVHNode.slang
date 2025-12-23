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

// #define OLD
#ifdef OLD
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
									 BVHNode *nodes, Triangle *triangles, uint *tri_ids){
	BVHNode node_stack[20];
	int stack_index = 0;
	node_stack[stack_index++] = nodes[0];
	
	float closest_so_far = ray_t.max;

	bool hit = false;
	while(stack_index > 0){
		BVHNode node = node_stack[--stack_index];
		if(intersect_aabb(ray, node.aabb_min, node.aabb_max, closest_so_far)){
			// Leaf node
			if (node.prim_count > 0) {
				for (uint i = 0; i < node.prim_count; ++i) {
					if (triangle_hit(triangles[tri_ids[node.left_first + i]], ray, Interval(ray_t.min, closest_so_far), rec)) {
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
#else
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

bool intersect_bvh(Ray ray, uint node_idx, Interval ray_t, inout HitRecord rec,
									 BVHNode *nodes, Triangle *triangles, uint *tri_ids) {
	uint node_stack[64];
	node_stack[0] = 0;
	uint stack_ptr = 0;
	float closest_so_far = ray_t.max;
	bool hit = false;

	while (true) {
		BVHNode node = nodes[node_stack[stack_ptr]];
		if (node.prim_count > 0) {
			for (uint i = 0; i < node.prim_count; ++i) {
				if (triangle_hit(triangles[tri_ids[node.left_first + i]], ray, Interval(ray_t.min, closest_so_far), rec)) {
					hit = true;
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
#endif // OLD
