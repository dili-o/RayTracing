#pragma once
#include "HitRecord.cuh"
#include "Interval.cuh"
#include "Material.cuh"
#include "Ray.cuh"
#include "Triangle.cuh"
// Vendor
#include <cuda/std/bit>

#define HD __host__ __device__

HD static uchar4 as_uchar4(const float v) {
  return cuda::std::bit_cast<uchar4>(v);
}

__device__ static const float BVH_FAR = 1e30f;

HD float safe_rcp(float x) {
  return (abs(x) > 1e-12f) ? (1.0f / x) : (x >= 0 ? BVH_FAR : -BVH_FAR);
}

HD float3 safe_rcp(float3 a) {
  return float3(safe_rcp(a.x), safe_rcp(a.y), safe_rcp(a.z));
}

HD uint32_t as_uint(const float v) { return cuda::std::bit_cast<uint32_t>(v); }

#define SWAP(A, B, C, D)                                                       \
  do {                                                                         \
    tmp = A;                                                                   \
    A = B;                                                                     \
    B = tmp;                                                                   \
    tmp2 = C;                                                                  \
    C = D;                                                                     \
    D = tmp2;                                                                  \
  } while (false)

// code compaction: Moeller-Trumbore ray/tri test.
#define MOLLER_TRUMBORE_TEST(tmax, exit)                                       \
  const float3 h = cross(ray.direction, e2);                                   \
  const float a = dot(e1, h);                                                  \
  if (fabs(a) < 0.000001f)                                                     \
    exit;                                                                      \
  const float f = 1 / a;                                                       \
  const float3 s = ray.origin - v0;                                            \
  const float u = f * dot(s, h);                                               \
  const float3 q = cross(s, e1);                                               \
  const float v = f * dot(ray.direction, q);                                   \
  const bool miss = u < 0 || v < 0 || u + v > 1;                               \
  if (miss)                                                                    \
    exit;                                                                      \
  const float t = f * dot(e2, q);                                              \
  if (t < 0 || t > tmax)                                                       \
    exit;

__device__ float intersect_aabb(const Ray &ray, const float3 &bmin,
                                const float3 &bmax, const float t) {
  float tx1 = (bmin.x - ray.origin.x) / ray.direction.x;
  float tx2 = (bmax.x - ray.origin.x) / ray.direction.x;
  float tmin = fminf(tx1, tx2);
  float tmax = fmaxf(tx1, tx2);
  float ty1 = (bmin.y - ray.origin.y) / ray.direction.y;
  float ty2 = (bmax.y - ray.origin.y) / ray.direction.y;
  tmin = fmaxf(tmin, fminf(ty1, ty2));
  tmax = fminf(tmax, fmaxf(ty1, ty2));
  float tz1 = (bmin.z - ray.origin.z) / ray.direction.z;
  float tz2 = (bmax.z - ray.origin.z) / ray.direction.z;
  tmin = fmaxf(tmin, fminf(tz1, tz2));
  tmax = fminf(tmax, fmaxf(tz1, tz2));
  if (tmax >= tmin && tmin < t && tmax > 0)
    return tmin;
  else
    return 1e30f;
}

// From tiny_bvh::BVH4_GPU
struct BVH4Node {
  struct aabb8 {
    uint8_t xmin, ymin, zmin, xmax, ymax, zmax;
  }; // quantized
  float3 aabbMin;
  uint32_t c0Info; // 16
  float3 aabbExt;
  uint32_t c1Info; // 16
  aabb8 c0bounds, c1bounds;
  uint32_t c2Info; // 16
  aabb8 c2bounds, c3bounds;
  uint32_t c3Info; // 16; total: 64 bytes
};

struct alignas(16) BLAS {
  uint32_t bvh4_data_offset;
  uint32_t tri_surf_data_offset;
  uint32_t padding_0;
  uint32_t padding_1;

  __device__ bool intersect4(const Ray &ray, const Interval &ray_t,
                             HitRecord &rec, float4 *bvh4_data) {
    bool hit = false;
    float closest_so_far = ray_t.max;

    // traverse a blas
    uint offset = 0, stack[128], stackPtr = 0, tmp2 /* for SWAP macro */;
    while (true) {
      // NOTE: Adding the bvh4_data_offset
      offset += bvh4_data_offset;

      // fetch the node
      const float4 &data0 = bvh4_data[offset + 0],
                   &data1 = bvh4_data[offset + 1];
      const float4 &data2 = bvh4_data[offset + 2],
                   &data3 = bvh4_data[offset + 3];
      // extract aabb
      const float3 bmin = float3(data0.x, data0.y, data0.z),
                   extent = float3(data1.x, data1.y,
                                   data1.z); // pre-scaled by 1 / 255

      // reconstruct conservative child aabbs
      const uchar4 d0 = as_uchar4(data0.w), d1 = as_uchar4(data1.w),
                   d2 = as_uchar4(data2.x);
      const uchar4 d3 = as_uchar4(data2.y), d4 = as_uchar4(data2.z),
                   d5 = as_uchar4(data2.w);
      const float3 c0min = bmin + extent * float3(d0.x, d2.x, d4.x),
                   c0max = bmin + extent * float3(d1.x, d3.x, d5.x);
      const float3 c1min = bmin + extent * float3(d0.y, d2.y, d4.y),
                   c1max = bmin + extent * float3(d1.y, d3.y, d5.y);
      const float3 c2min = bmin + extent * float3(d0.z, d2.z, d4.z),
                   c2max = bmin + extent * float3(d1.z, d3.z, d5.z);
      const float3 c3min = bmin + extent * float3(d0.w, d2.w, d4.w),
                   c3max = bmin + extent * float3(d1.w, d3.w, d5.w);

      // intersect child aabbs
      const float3 r_direction = safe_rcp(ray.direction);
      const float3 t1a = (c0min - ray.origin) * r_direction,
                   t2a = (c0max - ray.origin) * r_direction;
      const float3 t1b = (c1min - ray.origin) * r_direction,
                   t2b = (c1max - ray.origin) * r_direction;
      const float3 t1c = (c2min - ray.origin) * r_direction,
                   t2c = (c2max - ray.origin) * r_direction;
      const float3 t1d = (c3min - ray.origin) * r_direction,
                   t2d = (c3max - ray.origin) * r_direction;
      const float3 minta = fminf(t1a, t2a), maxta = fmaxf(t1a, t2a);
      const float3 mintb = fminf(t1b, t2b), maxtb = fmaxf(t1b, t2b);
      const float3 mintc = fminf(t1c, t2c), maxtc = fmaxf(t1c, t2c);
      const float3 mintd = fminf(t1d, t2d), maxtd = fmaxf(t1d, t2d);

      const float tmina = fmaxf(fmaxf(fmaxf(minta.x, minta.y), minta.z), 0.0f);
      const float tminb = fmaxf(fmaxf(fmaxf(mintb.x, mintb.y), mintb.z), 0.0f);
      const float tminc = fmaxf(fmaxf(fmaxf(mintc.x, mintc.y), mintc.z), 0.0f);
      const float tmind = fmaxf(fmaxf(fmaxf(mintd.x, mintd.y), mintd.z), 0.0f);
      const float tmaxa = fminf(fminf(fminf(maxta.x, maxta.y), maxta.z), rec.t);
      const float tmaxb = fminf(fminf(fminf(maxtb.x, maxtb.y), maxtb.z), rec.t);
      const float tmaxc = fminf(fminf(fminf(maxtc.x, maxtc.y), maxtc.z), rec.t);
      const float tmaxd = fminf(fminf(fminf(maxtd.x, maxtd.y), maxtd.z), rec.t);
      float dist0 = tmina > tmaxa ? BVH_FAR : tmina,
            dist1 = tminb > tmaxb ? BVH_FAR : tminb;
      float dist2 = tminc > tmaxc ? BVH_FAR : tminc,
            dist3 = tmind > tmaxd ? BVH_FAR : tmind, tmp;
      // get child node info fields
      uint c0info = as_uint(data3.x), c1info = as_uint(data3.y);
      uint c2info = as_uint(data3.z), c3info = as_uint(data3.w);
      if (dist0 < dist2)
        SWAP(dist0, dist2, c0info, c2info);
      if (dist1 < dist3)
        SWAP(dist1, dist3, c1info, c3info);
      if (dist0 < dist1)
        SWAP(dist0, dist1, c0info, c1info);
      if (dist2 < dist3)
        SWAP(dist2, dist3, c2info, c3info);
      if (dist1 < dist2)
        SWAP(dist1, dist2, c1info, c2info);
      // process results, starting with farthest child, so nearest ends on top
      // of stack

      uint nextNode = 0;
      uint leaf[4] = {0, 0, 0, 0}, leafs = 0;
      if (dist0 < BVH_FAR) {
        if (bool(c0info & 0x80000000))
          leaf[leafs++] = c0info;
        else if (bool(c0info))
          stack[stackPtr++] = c0info;
      }
      if (dist1 < BVH_FAR) {
        if (bool(c1info & 0x80000000))
          leaf[leafs++] = c1info;
        else if (bool(c1info))
          stack[stackPtr++] = c1info;
      }
      if (dist2 < BVH_FAR) {
        if (bool(c2info & 0x80000000))
          leaf[leafs++] = c2info;
        else if (bool(c2info))
          stack[stackPtr++] = c2info;
      }
      if (dist3 < BVH_FAR) {
        if (bool(c3info & 0x80000000))
          leaf[leafs++] = c3info;
        else if (bool(c3info))
          stack[stackPtr++] = c3info;
      }

      // process encountered leafs, if any
      for (uint i = 0; i < leafs; i++) {
        const uint N = (leaf[i] >> 16) & 0x7fff;
        uint triStart = offset + (leaf[i] & 0xffff);
        for (uint j = 0; j < N; j++, triStart += 3) {
          // cost += c_int;
          const float3 e2 = make_float3(bvh4_data[triStart + 2]);
          const float3 e1 = make_float3(bvh4_data[triStart + 1]);
          const float3 v0 = make_float3(bvh4_data[triStart + 0]);
          MOLLER_TRUMBORE_TEST(closest_so_far, continue);
          rec.t = t;
          rec.p = ray.at(t);
          rec.u = u;
          rec.v = v;
          float3 normal = normalize(cross(e1, e2));
          rec.set_face_normal(ray, normal);

          hit = true;
          closest_so_far = rec.t;
          uint tri_id = as_uint(bvh4_data[triStart + 0].w);
          rec.tri_surface_offset =
              tri_surf_data_offset +
              (sizeof(TriangleShading) / sizeof(float4)) * tri_id;
        }
      }

      // continue with nearest node or first node on the stack
      if (bool(nextNode))
        offset = nextNode;
      else {
        if (!bool(stackPtr))
          break;
        offset = stack[--stackPtr];
      }
    }
    return hit;
  }
};

HD struct float4x4 {
  float4 cols[4]; // Each float4 is a COLUMN

  // Constructor: Matches GLM's column-by-column initialization
  HD float4x4(float4 c0, float4 c1, float4 c2, float4 c3) {
    cols[0] = c0;
    cols[1] = c1;
    cols[2] = c2;
    cols[3] = c3;
  }

  // Standard constructor using 16 floats (column-major order)
  HD float4x4(float m00, float m10, float m20, float m30, // Col 0
              float m01, float m11, float m21, float m31, // Col 1
              float m02, float m12, float m22, float m32, // Col 2
              float m03, float m13, float m23, float m33) // Col 3
  {
    cols[0] = make_float4(m00, m10, m20, m30);
    cols[1] = make_float4(m01, m11, m21, m31);
    cols[2] = make_float4(m02, m12, m22, m32);
    cols[3] = make_float4(m03, m13, m23, m33);
  }
};

// Overload for Matrix * Vector (float4x4 * float4)
// This implementation is efficient for column-major storage
HD inline float4 operator*(const float4x4 &m, const float4 &v) {
  return make_float4(m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z +
                         m.cols[3].x * v.w,
                     m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z +
                         m.cols[3].y * v.w,
                     m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z +
                         m.cols[3].z * v.w,
                     m.cols[0].w * v.x + m.cols[1].w * v.y + m.cols[2].w * v.z +
                         m.cols[3].w * v.w);
}

HD inline float4x4 transpose(const float4x4 &m) {
  return float4x4(m.cols[0].x, m.cols[1].x, m.cols[2].x, m.cols[3].x,
                  m.cols[0].y, m.cols[1].y, m.cols[2].y, m.cols[3].y,
                  m.cols[0].z, m.cols[1].z, m.cols[2].z, m.cols[3].z,
                  m.cols[0].w, m.cols[1].w, m.cols[2].w, m.cols[3].w);
}

struct alignas(16) BLASInstance {
  float4x4 transform;
  float4x4 inv_transform;

  MaterialHandle material_handle;
  uint32_t blas_index;
  uint32_t padding;

  __device__ bool intersect(const Ray &ray, const Interval ray_t,
                            HitRecord &rec, BLAS *blases, float4 *bvh4_data) {
    float4 transformed_origin =
        inv_transform * float4(ray.origin.x, ray.origin.y, ray.origin.z, 1.f);
    float4 transformed_dir =
        inv_transform *
        float4(ray.direction.x, ray.direction.y, ray.direction.z, 0.f);

    Ray world_ray(
        float3(transformed_origin.x, transformed_origin.y,
               transformed_origin.z),
        float3(transformed_dir.x, transformed_dir.y, transformed_dir.z));

    return blases[blas_index].intersect4(world_ray, ray_t, rec, bvh4_data);
  }
};
