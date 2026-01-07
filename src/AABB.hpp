#pragma once
#include "Vec3.hpp"

struct AABB {
  AABB() : min(infinity), max(-infinity) {}

  void grow(const Vec3 &p) {
    min = Vec3::fmin(min, p);
    max = Vec3::fmax(max, p);
  }

  void grow(const AABB &b) {
    min = Vec3::fmin(min, b.min);
    max = Vec3::fmax(max, b.max);
  }

  f32 half_area() {
    Vec3 e = max - min;
    return e.x * e.y + e.y * e.z + e.x * e.z;
  }

  Vec3 min;
  Vec3 max;
};

static f32 intersect_aabb(const Ray &ray, const Vec3 &bmin, const Vec3 &bmax,
                   const f32 t) {
  f32 tx1 = (bmin.x - ray.origin.x) * ray.inv_direction.x,
      tx2 = (bmax.x - ray.origin.x) * ray.inv_direction.x;
  f32 tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
  f32 ty1 = (bmin.y - ray.origin.y) * ray.inv_direction.y,
      ty2 = (bmax.y - ray.origin.y) * ray.inv_direction.y;
  tmin = std::max(tmin, std::min(ty1, ty2)),
  tmax = std::min(tmax, std::max(ty1, ty2));
  f32 tz1 = (bmin.z - ray.origin.z) * ray.inv_direction.z,
      tz2 = (bmax.z - ray.origin.z) * ray.inv_direction.z;
  tmin = std::max(tmin, std::min(tz1, tz2)),
  tmax = std::min(tmax, std::max(tz1, tz2));
  if (tmax >= tmin && tmin < t && tmax > 0)
    return tmin;
  else
    return infinity;
}
