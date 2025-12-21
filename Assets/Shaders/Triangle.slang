#pragma once
#include "HitRecord.slang"
#include "Interval.slang"

struct Triangle {
  float4 v0;
  float4 v1;
  float4 v2;

  float4 n0;
  float4 n1;
  float4 n2;

  float2 uv_0;
  float2 uv_1;

  float2 uv_2;
  uint material_index;
  uint material_type;
};

bool triangle_hit(in Triangle triangle, in Ray r, Interval ray_t, inout HitRecord rec) {
	const float3 v0 = triangle.v0.xyz;
	const float3 v1 = triangle.v1.xyz;
	const float3 v2 = triangle.v2.xyz;

	const float3 n0 = triangle.n0.xyz;
	const float3 n1 = triangle.n1.xyz;
	const float3 n2 = triangle.n2.xyz;

	const float3 edge1 = v1 - v0;
	const float3 edge2 = v2 - v0;
	const float3 h = cross(r.direction, edge2);
	const float a = dot(edge1, h);

	if (a > -EPSILON && a < EPSILON)
		return false; // ray parallel to triangle

	const float f = 1.f / a;
	const float3 s = r.origin - v0;
	const float u = f * dot(s, h);

	if (u < 0 || u > 1) 
		return false;

	const float3 q = cross(s, edge1);
	const float v = f * dot(r.direction, q);

	if (v < 0 || u + v > 1) 
		return false;

	const float t = f * dot(edge2, q);

	if (t < EPSILON || !ray_t.contains(t))
		return false;

	rec.t = t;
	rec.p = r.at(t);

	// Use the geometric normal to set the face
	float3 geom_normal = normalize(cross(edge1, edge2));
	rec.set_face_normal(r, geom_normal);

	// Use the interpolated normal to set the outward normal
	const float alpha = 1.f - u - v;
	float3 interpolated_normal = normalize(alpha * n0 + u * n1 + v * n2);
	rec.normal = interpolated_normal;

	// Get UV
	const float2 uv_0 = triangle.uv_0;
	const float2 uv_1 = triangle.uv_1;
	const float2 uv_2 = triangle.uv_2;
	rec.u = alpha * uv_0.x + u * uv_1.x + v * uv_2.x;
	rec.v = alpha * uv_0.y + u * uv_1.y + v * uv_2.y;

	rec.material_index = triangle.material_index;
	rec.material_type = triangle.material_type;
	return true;
}


