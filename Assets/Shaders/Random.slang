#pragma once

static const float pi = 3.14159265359f;
static const float two_pi = 2.0f * pi;

// Random Number Generators
uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

// Range 0 to 1
float rand(inout uint seed) {
    return float(wang_hash(seed)) / 4294967296.0;
}

float rand_range(inout uint seed, float min, float max) {
  return min + (max - min) * rand(seed);
}

float3 rand_float3(inout uint seed) {
  return float3(rand(seed), rand(seed), rand(seed));
}

float3 rand_float3_range(inout uint seed, float min, float max) {
  return float3(rand_range(seed, min, max), rand_range(seed, min, max),
              rand_range(seed, min, max));
}

float3 rand_unit_vector(inout uint seed) {
  float z = rand_range(seed, -1.f, 1.f);
  float a = rand(seed) * two_pi;
  float r = sqrt(1.f - z * z);
  float x = r * cos(a);
  float y = r * sin(a);
  return float3(x, y, z);
}

float3 rand_on_hemisphere(inout uint seed, float3 normal) {
  float3 on_unit_sphere = rand_unit_vector(seed);
  if (dot(on_unit_sphere, normal) > 0.f) // In the same hemisphere as the normal
    return on_unit_sphere;
  else
    return -on_unit_sphere;
}

inline bool near_zero(float3 v) {
  // Return true if the vector is close to zero in all dimensions.
  float s = 1e-8;
  return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

inline float3 rand_in_unit_disk(inout uint seed) {
  while (true) {
      float3 p = float3(rand_range(seed, -1.f, 1.f), rand_range(seed, -1.f,1.f), 0.f);
      if (dot(p, p) < 1.f)
          return p;
  }
}

inline float3 defocus_disk_sample(inout uint seed, float3 center,
                                  float3 defocus_disk_u, float3 defocus_disk_v) {
  // Returns a random point in the camera defocus disk.
  float3 p = rand_in_unit_disk(seed);
  return (center + (p.x * defocus_disk_u) + (p.y * defocus_disk_v));
}
