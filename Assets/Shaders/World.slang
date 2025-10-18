#pragma once
#include "Ray.slang"
#include "Sphere.slang"

bool hit_world(Ray r, Interval ray_t, inout HitRecord rec, Sphere *spheres,
               uint sphere_count) {
  float closest_so_far = ray_t.max;
  bool hit_sphere = false;
  for (uint i = 0; i < sphere_count; ++i) {
    Sphere sphere = spheres[i];
    if (sphere_hit(sphere, r, Interval(ray_t.min, closest_so_far), rec)) {
      hit_sphere = true;
      closest_so_far = rec.t;
    }
  }

  return hit_sphere;
}
