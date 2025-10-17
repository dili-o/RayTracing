#ifndef VEC3_H
#define VEC3_H

#include "Assert.hpp"
#include "Defines.hpp"

#include <cmath>
#include <iostream>

class Vec3 {
public:
  real e[3];

  Vec3() : e{0, 0, 0} {}
  Vec3(real e0, real e1, real e2) : e{e0, e1, e2} {}

  real x() const { return e[0]; }
  real y() const { return e[1]; }
  real z() const { return e[2]; }

  Vec3 operator-() const { return Vec3(-e[0], -e[1], -e[2]); }
  real operator[](int i) const { return e[i]; }
  real &operator[](int i) { return e[i]; }

  Vec3 &operator+=(const Vec3 &v) {
    e[0] += v.e[0];
    e[1] += v.e[1];
    e[2] += v.e[2];
    return *this;
  }

  Vec3 &operator*=(real t) {
    e[0] *= t;
    e[1] *= t;
    e[2] *= t;
    return *this;
  }

  static void set_float4(f32 float4[4], const Vec3 &vec) {
    float4[0] = vec.x();
    float4[1] = vec.y();
    float4[2] = vec.z();
  }

  Vec3 &operator/=(real t) { return *this *= 1 / t; }

  real length() const { return std::sqrt(length_squared()); }

  real length_squared() const {
    return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
  }

  bool near_zero() const {
    // Return true if the vector is close to zero in all dimensions.
    real s = 1e-8f;
    return (std::fabs(e[0]) < s) && (std::fabs(e[1]) < s) &&
           (std::fabs(e[2]) < s);
  }

  static Vec3 random() {
    return Vec3(random_real(), random_real(), random_real());
  }

  static Vec3 random(real min, real max) {
    return Vec3(random_real(min, max), random_real(min, max),
                random_real(min, max));
  }
};

// Point3 is just an alias for Vec3, but useful for geometric clarity in the
// code.
using Point3 = Vec3;
using Color = Vec3;

// Vector Utility Functions

inline std::ostream &operator<<(std::ostream &out, const Vec3 &v) {
  return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline Vec3 operator+(const Vec3 &u, const Vec3 &v) {
  return Vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline Vec3 operator-(const Vec3 &u, const Vec3 &v) {
  return Vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline Vec3 operator*(const Vec3 &u, const Vec3 &v) {
  return Vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline Vec3 operator*(real t, const Vec3 &v) {
  return Vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline Vec3 operator*(const Vec3 &v, real t) { return t * v; }

inline Vec3 operator/(const Vec3 &v, real t) { return (1 / t) * v; }

inline real dot(const Vec3 &u, const Vec3 &v) {
  return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

inline Vec3 cross(const Vec3 &u, const Vec3 &v) {
  return Vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
              u.e[2] * v.e[0] - u.e[0] * v.e[2],
              u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline Vec3 unit_vector(const Vec3 &v) { return v / v.length(); }

inline Vec3 random_unit_vector() {
  while (true) {
    Vec3 p = Vec3::random(-1.f, 1.f);
    real lensq = p.length_squared();
    if (1e-6f < lensq && lensq <= 1.f)
      return (p / sqrt(lensq));
  }
}

inline Vec3 random_on_hemisphere(const Vec3 &normal) {
  Vec3 on_unit_sphere = random_unit_vector();
  if (dot(normal, on_unit_sphere) > 0.f)
    return on_unit_sphere;
  else
    return -on_unit_sphere;
}

inline Vec3 reflect(const Vec3 &v, const Vec3 &n) {
  return v - 2.f * dot(v, n) * n;
}

inline Vec3 refract(const Vec3 &uv, const Vec3 &n, real etai_over_etat) {
  real cos_theta = std::fmin(dot(-uv, n), 1.f);
  Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
  Vec3 r_out_parallel =
      -std::sqrt(std::fabs(1.f - r_out_perp.length_squared())) * n;
  return r_out_perp + r_out_parallel;
}
#endif
