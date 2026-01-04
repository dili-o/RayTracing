#pragma once
#include "Vec3.hpp"
#include "Vec4.hpp"

// Row major matrix class
class Mat4 {
public:
  union {
    struct {
      f32 row0[4];
      f32 row1[4];
      f32 row2[4];
      f32 row3[4];
    };

    f32 e[16];
  };

  Mat4()
      : e{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
          0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f} {}

  Mat4 &operator+=(const Mat4 &m) {
    for (i32 i = 0; i < 16; i++)
      e[i] += m.e[i];
    return *this;
  }

  Mat4 &operator-=(const Mat4 &m) {
    for (i32 i = 0; i < 16; i++)
      e[i] -= m.e[i];
    return *this;
  }

  bool operator==(const Mat4 &m) {
    for (i32 i = 0; i < 16; i++)
      if (m.e[i] != e[i])
        return false;
    return true;
  }

  Mat4 operator*(const Mat4 &b) const {
    Mat4 r;
    for (u32 i = 0; i < 16; i += 4) {
      for (u32 j = 0; j < 4; ++j) {
        r[i + j] = (e[i + 0] * b.e[j + 0]) + (e[i + 1] * b.e[j + 4]) +
                   (e[i + 2] * b.e[j + 8]) + (e[i + 3] * b.e[j + 12]);
      }
    }
    return r;
  }

  Vec4 operator*(const Vec4 &v) const {
    return Vec4(e[0] * v.x + e[1] * v.y + e[2] * v.z + e[3] * v.w,
                e[4] * v.x + e[5] * v.y + e[6] * v.z + e[7] * v.w,
                e[8] * v.x + e[9] * v.y + e[10] * v.z + e[11] * v.w,
                e[12] * v.x + e[13] * v.y + e[14] * v.z + e[15] * v.w);
  }

  f32 operator[](i32 i) const { return e[i]; }

  f32 &operator[](int i) { return e[i]; }

  Vec3 get_translation() const { return Vec3(row0[3], row1[3], row2[3]); }

  [[nodiscard]]
  Mat4 inverse() const {
    // from MESA, via
    // http://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    const f32 inv[16] = {
        e[5] * e[10] * e[15] - e[5] * e[11] * e[14] - e[9] * e[6] * e[15] +
            e[9] * e[7] * e[14] + e[13] * e[6] * e[11] - e[13] * e[7] * e[10],
        -e[1] * e[10] * e[15] + e[1] * e[11] * e[14] + e[9] * e[2] * e[15] -
            e[9] * e[3] * e[14] - e[13] * e[2] * e[11] + e[13] * e[3] * e[10],
        e[1] * e[6] * e[15] - e[1] * e[7] * e[14] - e[5] * e[2] * e[15] +
            e[5] * e[3] * e[14] + e[13] * e[2] * e[7] - e[13] * e[3] * e[6],
        -e[1] * e[6] * e[11] + e[1] * e[7] * e[10] + e[5] * e[2] * e[11] -
            e[5] * e[3] * e[10] - e[9] * e[2] * e[7] + e[9] * e[3] * e[6],
        -e[4] * e[10] * e[15] + e[4] * e[11] * e[14] + e[8] * e[6] * e[15] -
            e[8] * e[7] * e[14] - e[12] * e[6] * e[11] + e[12] * e[7] * e[10],
        e[0] * e[10] * e[15] - e[0] * e[11] * e[14] - e[8] * e[2] * e[15] +
            e[8] * e[3] * e[14] + e[12] * e[2] * e[11] - e[12] * e[3] * e[10],
        -e[0] * e[6] * e[15] + e[0] * e[7] * e[14] + e[4] * e[2] * e[15] -
            e[4] * e[3] * e[14] - e[12] * e[2] * e[7] + e[12] * e[3] * e[6],
        e[0] * e[6] * e[11] - e[0] * e[7] * e[10] - e[4] * e[2] * e[11] +
            e[4] * e[3] * e[10] + e[8] * e[2] * e[7] - e[8] * e[3] * e[6],
        e[4] * e[9] * e[15] - e[4] * e[11] * e[13] - e[8] * e[5] * e[15] +
            e[8] * e[7] * e[13] + e[12] * e[5] * e[11] - e[12] * e[7] * e[9],
        -e[0] * e[9] * e[15] + e[0] * e[11] * e[13] + e[8] * e[1] * e[15] -
            e[8] * e[3] * e[13] - e[12] * e[1] * e[11] + e[12] * e[3] * e[9],
        e[0] * e[5] * e[15] - e[0] * e[7] * e[13] - e[4] * e[1] * e[15] +
            e[4] * e[3] * e[13] + e[12] * e[1] * e[7] - e[12] * e[3] * e[5],
        -e[0] * e[5] * e[11] + e[0] * e[7] * e[9] + e[4] * e[1] * e[11] -
            e[4] * e[3] * e[9] - e[8] * e[1] * e[7] + e[8] * e[3] * e[5],
        -e[4] * e[9] * e[14] + e[4] * e[10] * e[13] + e[8] * e[5] * e[14] -
            e[8] * e[6] * e[13] - e[12] * e[5] * e[10] + e[12] * e[6] * e[9],
        e[0] * e[9] * e[14] - e[0] * e[10] * e[13] - e[8] * e[1] * e[14] +
            e[8] * e[2] * e[13] + e[12] * e[1] * e[10] - e[12] * e[2] * e[9],
        -e[0] * e[5] * e[14] + e[0] * e[6] * e[13] + e[4] * e[1] * e[14] -
            e[4] * e[2] * e[13] - e[12] * e[1] * e[6] + e[12] * e[2] * e[5],
        e[0] * e[5] * e[10] - e[0] * e[6] * e[9] - e[4] * e[1] * e[10] +
            e[4] * e[2] * e[9] + e[8] * e[1] * e[6] - e[8] * e[2] * e[5]};
    const f32 det =
        e[0] * inv[0] + e[1] * inv[4] + e[2] * inv[8] + e[3] * inv[12];
    Mat4 ret_val;
    if (det != 0) {
      const f32 inv_det = 1.0f / det;
      for (i32 i = 0; i < 16; i++)
        ret_val.e[i] = inv[i] * inv_det;
    }
    return ret_val;
  }

  static Mat4 identity() { return Mat4{}; }

  static Mat4 translate(const Vec3 &v) {
    Mat4 mat{};
    mat.row0[3] = v.x;
    mat.row1[3] = v.y;
    mat.row2[3] = v.z;
    return mat;
  }

  static Mat4 rotate_x(const f32 a) {
    Mat4 m;
    m.e[5] = cosf(a);
    m.e[6] = -sinf(a);
    m.e[9] = sinf(a);
    m.e[10] = cosf(a);
    return m;
  };

  static Mat4 rotate_y(const f32 a) {
    Mat4 m;
    m.e[0] = cosf(a);
    m.e[2] = sinf(a);
    m.e[8] = -sinf(a);
    m.e[10] = cosf(a);
    return m;
  };

  static Mat4 rotate_z(const f32 a) {
    Mat4 m;
    m.e[0] = cosf(a);
    m.e[1] = -sinf(a);
    m.e[4] = sinf(a);
    m.e[5] = cosf(a);
    return m;
  };

  static Mat4 scale(const f32 s) {
    Mat4 m;
    m.e[0] = m.e[5] = m.e[10] = s;
    return m;
  }

  static Mat4 scale(const Vec3 &s) {
    Mat4 m;
    m.e[0] = s.x, m.e[5] = s.y, m.e[10] = s.z;
    return m;
  }
};
