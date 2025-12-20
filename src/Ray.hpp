#ifndef RAY_H
#define RAY_H

#include "Vec3.hpp"

struct Ray {
  Ray() {}

  Ray(const Point3 &origin, const Vec3 &direction)
      : origin(origin), direction(direction) {}

  Point3 at(real t) const { return origin + t * direction; }

  Point3 origin;
  Vec3 direction;
};

#endif
