#ifndef RAY_H
#define RAY_H

#include "Vec3.hpp"

struct Ray {
  Ray() {}

  Point3 origin;
  Vec3 direction;
  Vec3 inv_direction;

  Ray(const Point3 &origin, const Vec3 &direction)
      : origin(origin), direction(direction),
        inv_direction(1.f / direction.x, 1.f / direction.y, 1.f / direction.z) {}

  Point3 at(real t) const { return origin + t * direction; }

};

#endif
