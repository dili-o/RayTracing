#ifndef SPHERE_H
#define SPHERE_H

#include "Hittable.hpp"
#include "Vec3.hpp"

class Sphere : public Hittable {
public:
  Sphere(const Point3 &center, real radius, std::shared_ptr<Material> mat)
      : center(center), radius(std::fmax(0.f, radius)), mat(mat) {}

  bool hit(const Ray &r, Interval ray_t, HitRecord &rec) const override {
    Vec3 oc = center - r.origin();
    real a = r.direction().length_squared();
    real h = dot(r.direction(), oc);
    real c = oc.length_squared() - radius * radius;

    real discriminant = h * h - a * c;
    if (discriminant < 0.f)
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
    get_uv(outward_normal, rec.u, rec.v);
    rec.mat = mat;

    return true;
  }

private:
  static void get_uv(const Point3& p, real& u, real& v) {
		// p: a given point on the sphere of radius one, centered at the origin.
		// u: returned value [0,1] of angle around the Y axis from X=-1.
		// v: returned value [0,1] of angle from Y=-1 to Y=+1.
		//     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
		//     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
		//     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

		auto theta = std::acos(-p.y);
		auto phi = std::atan2(-p.z, p.x) + pi;

		u = phi / (2*pi);
		v = theta / pi;
  }

private:
  Point3 center;
  real radius;
  std::shared_ptr<Material> mat;
};

struct alignas(16) SphereGPU {
  SphereGPU(const Point3 &origin, f32 radius, u32 material_index,
               u32 material_type)
      : origin(origin), radius(radius), material_index(material_index),
        material_type(material_type) {}
  Point3 origin;
  f32 radius;

  u32 material_index;
  u32 material_type;
  u32 padding[2];
};

#endif
