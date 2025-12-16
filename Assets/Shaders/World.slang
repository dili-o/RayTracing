#pragma once
#include "Ray.slang"
#include "Sphere.slang"
#include "Triangle.slang"

bool hit_world(Ray r, Interval ray_t, inout HitRecord rec,
               Sphere *spheres, uint sphere_count,
               Triangle *triangles, uint triangle_count) {
  float closest_so_far = ray_t.max;
  bool hit = false;
  for (uint i = 0; i < sphere_count; ++i) {
    Sphere sphere = spheres[i];
    if (sphere_hit(sphere, r, Interval(ray_t.min, closest_so_far), rec)) {
      hit = true;
      closest_so_far = rec.t;
    }
  }
  for (uint i = 0; i < triangle_count; ++i) {
    Triangle triangle = triangles[i];
    if (triangle_hit(triangle, r, Interval(ray_t.min, closest_so_far), rec)) {
      hit = true;
      closest_so_far = rec.t;
    }
  }

  return hit;
}
