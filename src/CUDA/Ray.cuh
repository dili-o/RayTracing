#pragma once
// Vendor
#include <helper_math.h>

__device__ const float EPSILON = 1.192092896e-07f;

struct Ray {
  __device__ Ray() {}
  __device__ Ray(const float3 &origin_, const float3 &direction_) {
    origin = origin_;
    direction = direction_;
  }

  __device__ float3 at(float t) const { return origin + t * direction; }

  float3 origin;
  float3 direction;
};
