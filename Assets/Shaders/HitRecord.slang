#pragma once

#include "Ray.slang"

struct HitRecord {
  float3 p;
  float3 normal;
  float t;
  bool front_face;

  [mutating]
  void set_face_normal(in Ray r,
                       in float3 outward_normal) {
    front_face = dot(r.direction, outward_normal) < 0.f;
    normal = front_face ? outward_normal : -outward_normal;
  }
};

