#ifndef SPHERE_H
#define SPHERE_H

#include "Hittable.hpp"
#include "Vec3.hpp"

class Sphere : public Hittable {
public:
  Sphere(const Point3 &center, real radius)
      : center(center), radius(std::fmax(0, radius)) {}

  bool hit(const Ray &r, Interval ray_t, HitRecord &rec) const override {
    Vec3 oc = center - r.origin();
    real a = r.direction().length_squared();
    real h = dot(r.direction(), oc);
    real c = oc.length_squared() - radius * radius;

    real discriminant = h * h - a * c;
    if (discriminant < 0)
      return false;

    real sqrtd = std::sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range.
    real root = (h - sqrtd) / a;
    if (!ray_t.surrounds(root)) {
      root = (h + sqrtd) / a;
      if (!ray_t.surrounds(root))
        return false;
    }

    rec.t = root;
    rec.p = r.at(rec.t);
    Vec3 outward_normal = (rec.p - center) / radius;
    rec.set_face_normal(r, outward_normal);

    return true;
  }

private:
  Point3 center;
  real radius;
};

struct alignas(16) GpuSphere {
  GpuSphere(const Point3 &origin, f32 radius)
      : origin(origin), radius(radius) {}
  Point3 origin;
  f32 radius;
};

#endif
