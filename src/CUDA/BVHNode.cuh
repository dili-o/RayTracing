#pragma once
#include "HitRecord.cuh"
#include "Interval.cuh"
#include "Material.cuh"
#include "Ray.cuh"
#include "Triangle.cuh"

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

struct alignas(16) BVHNode {
  float3 aabb_min;
  uint32_t local_left_first; // Points to the left node or the first prim

  float3 aabb_max;
  uint32_t tri_count;
};

struct alignas(16) BLAS {
  uint32_t bvh_nodes_offset;
  uint32_t nodes_count;
  // uint32_t tri_ids_offset;
  uint32_t tri_count_;
  uint32_t padding;

  __device__ bool intersect(const Ray &ray, Interval ray_t, HitRecord &rec,
                            BVHNode *nodes, TriangleGeom *tris,
                            uint32_t *tri_ids) {
    uint32_t node_id_stack[128];
    // Initialize first item in the stack to the root node
    node_id_stack[0] = bvh_nodes_offset;
    uint32_t stack_ptr = 0;
    float closest_so_far = ray_t.max;
    bool hit = false;

    while (true) {
      BVHNode node = nodes[node_id_stack[stack_ptr]];
      if (node.tri_count > 0) { // The node is a leaf
        for (uint32_t i = 0; i < node.tri_count; ++i) {
          uint32_t tri_id_index = node.local_left_first + i;
          uint32_t tri_index = tri_ids[tri_id_index];
          if ((tris[tri_index].hit(ray, Interval(ray_t.min, closest_so_far),
                                   rec))) {
            hit = true;
            // TODO: Fix
            rec.tri_geom_id = tri_index /**/;
            rec.tri_surface_id = tri_index;
            closest_so_far = rec.t;
          }
        }

        // Terminate when stack is empty
        if (stack_ptr == 0)
          break;
        else
          --stack_ptr;
      } else { // The node has children
        uint32_t child1_idx = bvh_nodes_offset + node.local_left_first;
        uint32_t child2_idx = child1_idx + 1;
        BVHNode child1 = nodes[child1_idx];
        BVHNode child2 = nodes[child2_idx];
        float dist1 = intersect_aabb(ray, child1.aabb_min, child1.aabb_max,
                                     closest_so_far);
        float dist2 = intersect_aabb(ray, child2.aabb_min, child2.aabb_max,
                                     closest_so_far);

        if (dist1 > dist2) {
          // Swap
          float temp = dist1;
          dist1 = dist2;
          dist2 = temp;
          // Swap
          uint32_t temp_c = child1_idx;
          child1_idx = child2_idx;
          child2_idx = temp_c;
        }

        if (dist1 == 1e30f) { // None of the aabbs were hit
          if (stack_ptr == 0)
            break;
          else
            --stack_ptr;
        } else {
          if (dist2 != 1e30f) {
            node_id_stack[stack_ptr++] = child2_idx;
          }
          node_id_stack[stack_ptr] = child1_idx;
        }
      }
    }

    return hit;
  }
};

#define HD __host__ __device__

__host__ __device__ struct float4x4 {
  float4 cols[4]; // Each float4 is a COLUMN

  // Constructor: Matches GLM's column-by-column initialization
  __host__ __device__ float4x4(float4 c0, float4 c1, float4 c2, float4 c3) {
    cols[0] = c0;
    cols[1] = c1;
    cols[2] = c2;
    cols[3] = c3;
  }

  // Standard constructor using 16 floats (column-major order)
  __host__ __device__
  float4x4(float m00, float m10, float m20, float m30, // Col 0
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
__host__ __device__ inline float4 operator*(const float4x4 &m,
                                            const float4 &v) {
  return make_float4(m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z +
                         m.cols[3].x * v.w,
                     m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z +
                         m.cols[3].y * v.w,
                     m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z +
                         m.cols[3].z * v.w,
                     m.cols[0].w * v.x + m.cols[1].w * v.y + m.cols[2].w * v.z +
                         m.cols[3].w * v.w);
}

__host__ __device__ inline float4x4 transpose(const float4x4 &m) {
  return float4x4(m.cols[0].x, m.cols[1].x, m.cols[2].x, m.cols[3].x,
                  m.cols[0].y, m.cols[1].y, m.cols[2].y, m.cols[3].y,
                  m.cols[0].z, m.cols[1].z, m.cols[2].z, m.cols[3].z,
                  m.cols[0].w, m.cols[1].w, m.cols[2].w, m.cols[3].w);
}

__host__ __device__ inline float3 to_float3(const float4 &v) {
  return make_float3(v.x, v.y, v.z);
}

struct alignas(16) BLASInstance {
  float4x4 transform;
  float4x4 inv_transform;

  MaterialHandle material_handle;
  uint32_t blas_index;
  uint32_t padding;

  __device__ bool intersect(const Ray &ray, const Interval ray_t,
                            HitRecord &rec, BLAS *blases, BVHNode *bvh_nodes,
                            TriangleGeom *tris, uint32_t *tri_ids) {
    float4 transformed_origin =
        inv_transform * float4(ray.origin.x, ray.origin.y, ray.origin.z, 1.f);
    float4 transformed_dir =
        inv_transform *
        float4(ray.direction.x, ray.direction.y, ray.direction.z, 0.f);

    Ray world_ray(
        float3(transformed_origin.x, transformed_origin.y,
               transformed_origin.z),
        float3(transformed_dir.x, transformed_dir.y, transformed_dir.z));

    return blases[blas_index].intersect(world_ray, ray_t, rec, bvh_nodes, tris,
                                        tri_ids);
  }
};
