#pragma once
// Vendor
#include <helper_math.h>

__device__ constexpr float PI = 3.14159265359f;
__device__ constexpr float TWO_PI = 2.f * PI;

// Random Number Generators
__device__ uint32_t wang_hash(uint32_t &seed) {
  seed = uint32_t(seed ^ uint32_t(61)) ^ uint32_t(seed >> uint32_t(16));
  seed *= uint32_t(9);
  seed = seed ^ (seed >> 4);
  seed *= uint32_t(0x27d4eb2d);
  seed = seed ^ (seed >> 15);
  return seed;
}

// Range 0.f to 1.f
__device__ float rand_float(uint32_t &seed) {
  return float(wang_hash(seed)) / 4294967296.f;
}

__device__ float rand_range(uint32_t &seed, float min, float max) {
  return min + (max - min) * rand_float(seed);
}

__device__ float3 rand_float3(uint32_t &seed) {
  return float3(rand_float(seed), rand_float(seed), rand_float(seed));
}

__device__ float3 rand_float3_range(uint32_t &seed, float min, float max) {
  return float3(rand_range(seed, min, max), rand_range(seed, min, max),
                rand_range(seed, min, max));
}

__device__ float3 rand_unit_vector(uint32_t &seed) {
  float z = rand_range(seed, -1.f, 1.f);
  float a = rand_float(seed) * TWO_PI;
  float r = sqrt(1.f - z * z);
  float x = r * cos(a);
  float y = r * sin(a);
  return float3(x, y, z);
}

__device__ float3 rand_on_hemisphere(uint32_t &seed, float3 &normal) {
  float3 on_unit_sphere = rand_unit_vector(seed);
  if (dot(on_unit_sphere, normal) > 0.f) // In the same hemisphere as the normal
    return on_unit_sphere;
  else
    return -on_unit_sphere;
}

__device__ inline bool near_zero(float3 &v) {
  // Return true if the vector is close to zero in all dimensions.
  float s = 1e-8;
  return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

__device__ inline float3 rand_in_unit_disk(uint32_t &seed) {
  while (true) {
    float3 p =
        float3(rand_range(seed, -1.f, 1.f), rand_range(seed, -1.f, 1.f), 0.f);
    if (dot(p, p) < 1.f)
      return p;
  }
}

__device__ inline float3 defocus_disk_sample(uint32_t &seed, float3 center,
                                             float3 &defocus_disk_u,
                                             float3 &defocus_disk_v) {
  // Returns a random point in the camera defocus disk.
  float3 p = rand_in_unit_disk(seed);
  return (center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v));
}
