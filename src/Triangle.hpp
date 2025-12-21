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

  bool hit(const Ray &r, const Interval ray_t, HitRecord &rec) const override {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 h = cross(r.direction, edge2);
    const f32 a = dot( edge1, h );
    constexpr f32 epsilon = std::numeric_limits<f32>::epsilon();

    if (a > -epsilon && a < epsilon)
      return false; // ray parallel to triangle

    const f32 f = 1.f / a;
    const Vec3 s = r.origin - v0;
    const f32 u = f * dot(s, h);

    if (u < 0 || u > 1) 
      return false;

    const Vec3 q = cross(s, edge1);
    const f32 v = f * dot(r.direction, q);

    if (v < 0 || u + v > 1) 
      return false;

    const f32 t = f * dot(edge2, q);

    if (t < epsilon || !ray_t.contains(t))
      return false;

    rec.t = t;
    rec.p = r.at(t);

    Vec3 outward_normal = unit_vector(cross(edge1, edge2));
    rec.set_face_normal(r, outward_normal);

    // Get UV
    const f32 alpha = 1.f - u - v;
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
  TriangleGPU(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
           Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
           MaterialHandle mat)
      : v0(v0), v1(v1), v2(v2),
        uv_0(uv_0), uv_1(uv_1), uv_2(uv_2),
        material_index(mat.index),
        material_type(mat.type) {}
  Vec3 v0; f32 pad_0; // 16
  Vec3 v1; f32 pad_1; // 16
  Vec3 v2; f32 pad_2; // 16
  Vec2 uv_0; //
  Vec2 uv_1; // 16

  Vec2 uv_2;          //
  u32 material_index; // 
  u32 material_type;  // 16
};
