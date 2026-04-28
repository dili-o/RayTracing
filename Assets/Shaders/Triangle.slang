#pragma once
#include "HitRecord.slang"
#include "Interval.slang"

struct TriangleGeom {
  bool hit(in Ray r, in Interval ray_t, inout HitRecord rec) {
    const float3 edge_1 = (v1 - v0).xyz;
    const float3 edge_2 = (v2 - v0).xyz;
    const float3 h = cross(r.direction, edge_2.xyz);
    const float a = dot(edge_1.xyz, h);

    if (a > -EPSILON && a < EPSILON)
      return false; // ray parallel to triangle

    const float f = 1.f / a;
    const float3 s = r.origin - v0.xyz;
    const float u = f * dot(s, h);

    if (u < 0 || u > 1) 
      return false;

    const float3 q = cross(s, edge_1.xyz);
    const float v = f * dot(r.direction, q);

    if (v < 0 || u + v > 1) 
      return false;

    const float t = f * dot(edge_2.xyz, q);

    if (t < EPSILON || !ray_t.contains(t))
      return false;

    rec.t = t;
    rec.p = r.at(t);
    rec.u = u;
    rec.v = v;

    float3 normal = normalize(cross(edge_1.xyz, edge_2.xyz));
    rec.set_face_normal(r, normal.xyz);

    return true;
  }

  float4 v0;
  float4 v1;
  float4 v2;
};

struct TriangleShading {
  float3 interpolate_normal(float u, float v) {
    float alpha = 1.f - u - v;
    return normalize(alpha * n0.xyz + u * n1.xyz + v * n2.xyz);
  }

  float2 interpolate_uvs(float u, float v) {
    float alpha = 1.f - u - v;
    return alpha * uv0 + u * uv1 + v * uv2;
  }

  float4 n0;
  float4 n1;
  float4 n2;
  float2 uv0;
  float2 uv1;
  float2 uv2;
  float2 padding;
};
