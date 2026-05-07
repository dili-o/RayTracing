#pragma once
#include "HitRecord.cuh"
#include "Interval.cuh"

struct alignas(16) TriangleGeom {
  __device__ bool hit(const Ray &r, const Interval ray_t, HitRecord &rec) {
    const float3 edge_1 = v1 - v0;
    const float3 edge_2 = v2 - v0;
    const float3 h = cross(r.direction, edge_2);
    const float a = dot(edge_1, h);

    if (a > -EPSILON && a < EPSILON)
      return false; // ray parallel to triangle

    const float f = 1.f / a;
    const float3 s = r.origin - v0;
    const float u = f * dot(s, h);

    if (u < 0 || u > 1)
      return false;

    const float3 q = cross(s, edge_1);
    const float v = f * dot(r.direction, q);

    if (v < 0 || u + v > 1)
      return false;

    const float t = f * dot(edge_2, q);

    if (t < EPSILON || !ray_t.contains(t))
      return false;

    rec.t = t;
    rec.p = r.at(t);
    rec.u = u;
    rec.v = v;

    float3 normal = normalize(cross(edge_1, edge_2));
    rec.set_face_normal(r, normal);

    return true;
  }

  float3 v0;
  float padding0;
  float3 v1;
  float padding1;
  float3 v2;
  float padding2;
};

struct alignas(16) TriangleShading {
  __device__ float3 interpolate_normal(float u, float v) {
    float alpha = 1.f - u - v;
    return normalize(alpha * n0 + u * n1 + v * n2);
  }

  __device__ float2 interpolate_uvs(float u, float v) {
    float alpha = 1.f - u - v;
    return alpha * uv0 + u * uv1 + v * uv2;
  }

  float3 n0;
  float padding0;
  float3 n1;
  float padding1;
  float3 n2;
  float padding2;

  float2 uv0;
  float2 uv1;
  float2 uv2;
  float2 padding;
};
