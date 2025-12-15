#pragma once

#include "Hittable.hpp"
#include "Vec3.hpp"
#include "Vec2.hpp"

class Triangle : public Hittable {
public:
  Triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
           Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
           std::shared_ptr<Material> mat)
      : v0(v0), v1(v1), v2(v2),
        uv_0(uv_0), uv_1(uv_1), uv_2(uv_2),
        mat(mat) {}

  bool hit(const Ray &r, Interval ray_t, HitRecord &rec) const override {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 h = cross(r.direction(), edge2);
    const f32 a = dot( edge1, h );
    constexpr f32 epsilon = std::numeric_limits<f32>::epsilon();

    if (a > -epsilon && a < epsilon)
      return false; // ray parallel to triangle

    const f32 f = 1.f / a;
    const Vec3 s = r.origin() - v0;
    const f32 u = f * dot(s, h);

    if (u < 0 || u > 1) 
      return false;

    const Vec3 q = cross(s, edge1);
    const f32 v = f * dot(r.direction(), q);

    if (v < 0 || u + v > 1) 
      return false;

    const f32 t = f * dot(edge2, q);

    if (!ray_t.contains(t))
      return false;

    rec.t = t;
    rec.p = r.at(t);

    Vec3 outward_normal = cross(edge1, edge2);
    rec.set_face_normal(r, outward_normal);

    // Get UV
    const f32 alpha = 1.0f - u - v;
    rec.u = alpha * uv_0.x + u * uv_1.x + v * uv_2.x;
    rec.v = alpha * uv_0.y + u * uv_1.y + v * uv_2.y;

    rec.mat = mat;
    return true;
  }

private:
  Vec3 v0;
  Vec3 v1;
  Vec3 v2;
  Vec2 uv_0;
  Vec2 uv_1;
  Vec2 uv_2;
  std::shared_ptr<Material> mat;
};

struct alignas(16) TriangleGPU {
  TriangleGPU(const Point3 &origin, f32 radius, u32 material_index,
               u32 material_type)
      : origin(origin), radius(radius), material_index(material_index),
        material_type(material_type) {}
  Point3 origin;
  f32 radius;

  u32 material_index;
  u32 material_type;
  u32 padding[2];
};
