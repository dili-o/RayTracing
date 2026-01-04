#pragma once

#include "Vec3.hpp"

class Vec4 {
public:
  Vec4() : x(0.f), y(0.f), z(0.f), w(0.f) {}
  Vec4(const Vec3 &v, f32 w) : x(v.x), y(v.y), z(v.z), w(w) {}
  Vec4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}

  union {
    struct {
      f32 x;
      f32 y;
      f32 z;
      f32 w;
    };

    f32 e[4];
  };
};

inline Vec3 make_vec3(const Vec4 &v) {
  return Vec3(v.x, v.y, v.z);
}
