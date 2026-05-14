#pragma once

#include "Ray.cuh"

struct HitRecord {
  float3 p;
  float3 normal;
  uint32_t tri_geom_id;
  uint32_t tri_surface_id;
  uint32_t blas_instance_id;
  float t;
  bool front_face;
  float u;
  float v;

  __device__ void set_face_normal(const Ray &r, float3 outward_normal) {
    front_face = dot(r.direction, outward_normal) < 0.f;
    normal = front_face ? outward_normal : -outward_normal;
  }
};
