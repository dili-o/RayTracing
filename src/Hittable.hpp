#ifndef HITTABLE_H
#define HITTABLE_H

#include "Interval.hpp"
#include "Ray.hpp"

class Material;

struct HitRecord {
  Point3 p;
  Vec3 normal;
  std::shared_ptr<Material> mat;
  real t;
  real u;
  real v;
  bool front_face;

  void set_face_normal(const Ray &r, const Vec3 &outward_normal) {
    front_face = dot(r.direction, outward_normal) < 0.f;
    normal = front_face ? outward_normal : -outward_normal;
  }
};

class Hittable {
public:
  virtual ~Hittable() = default;

  virtual bool hit(const Ray &r, Interval ray_t, HitRecord &rec) const = 0;
};

#endif
