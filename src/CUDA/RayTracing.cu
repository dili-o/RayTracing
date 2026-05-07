#include "HitRecord.cuh"
#include "Interval.cuh"
#include "Random.cuh"
#include "Ray.cuh"
#include "RayTracing.cuh"
#include "TLAS.cuh"

#define USE_CUDA_MATH
#include "Common.hpp"

// Vendor
#include <surface_indirect_functions.h>

static const u32 max_depth = 3;

__device__ float2 sample_square_stratified(u32 &seed, float recip_sqrt_spp,
                                           u32 s_i, u32 s_j) {
  // Returns the vector to a random point in the square sub-pixel specified by
  // grid indices s_i and s_j, for an idealized unit square pixel [-.5,-.5] to
  // [+.5,+.5].

  float px = ((s_i + rand_float(seed)) * recip_sqrt_spp) - 0.5f;
  float py = ((s_j + rand_float(seed)) * recip_sqrt_spp) - 0.5f;

  return float2(px, py);
}

__global__ void trace_world(void *raw_buf, const PushConstant constant_data) {
  int row = blockIdx.y * blockDim.y + threadIdx.y;
  int col = blockIdx.x * blockDim.x + threadIdx.x;

  int2 pixel_coord = int2(col, row);

  if (col >= constant_data.image_width || row >= constant_data.image_height)
    return;

  UniformData *data =
      reinterpret_cast<UniformData *>(constant_data.uniform_data_buffer);
  TLASNode *tlas_nodes = reinterpret_cast<TLASNode *>(data->tlas_nodes_buffer);
  BLASInstance *blas_instances =
      reinterpret_cast<BLASInstance *>(data->blas_instances_buffer);
  BLAS *blases = reinterpret_cast<BLAS *>(data->blas_buffer);
  BVHNode *bvh_nodes = reinterpret_cast<BVHNode *>(data->bvh_nodes_buffer);
  TriangleGeom *triangle_geoms =
      reinterpret_cast<TriangleGeom *>(data->triangle_geom_buffer);
  uint32_t *tri_ids = reinterpret_cast<uint32_t *>(data->tri_ids_buffer);
  TriangleShading *tri_shading_data =
      reinterpret_cast<TriangleShading *>(data->triangle_shading_buffer);

  LambertMaterial *lambert_materials =
      reinterpret_cast<LambertMaterial *>(data->lambert_materials_buffer);

  MetalMaterial *metal_materials =
      reinterpret_cast<MetalMaterial *>(data->metal_materials_buffer);

  DielectricMaterial *dielectric_materials =
      reinterpret_cast<DielectricMaterial *>(data->dielectric_materials_buffer);

  EmissiveMaterial *emissive_materials =
      reinterpret_cast<EmissiveMaterial *>(data->emissive_materials_buffer);

  u32 seed = pixel_coord.x * 1973u ^ pixel_coord.y * 9277u ^
             constant_data.frame_index * 26699u;
  float3 radiance = float3(0.f);

  for (u32 s_j = 0; s_j < constant_data.sqrt_spp; ++s_j) {
    for (u32 s_i = 0; s_i < constant_data.sqrt_spp; ++s_i) {
      float2 jitter = sample_square_stratified(
          seed, constant_data.recip_sqrt_spp, s_i, s_j);

      float3 pixel_sample = data->pixel00_loc +
                            ((pixel_coord.x + jitter.x) * data->pixel_delta_u) +
                            ((pixel_coord.y + jitter.y) * data->pixel_delta_v);

      float3 ray_direction =
          normalize(pixel_sample - float3(data->camera_center));
      Ray r(data->camera_center, ray_direction);

      float3 attenuation = float3(1.f, 1.f, 1.f);
      float3 sample_radiance = float3(0.f);

      for (uint d = 0; d < max_depth; ++d) {
        Interval ray_t = Interval(0.0001f, 1000.f);
        HitRecord rec;
        rec.t = 1000.f;
        float3 emission = float3(0.f);

        bool hit_anything =
            intersect_tlas(r, ray_t, rec, tlas_nodes, blas_instances, blases,
                           bvh_nodes, triangle_geoms, tri_ids);

        if (hit_anything) {
          BLASInstance blas_instance = blas_instances[rec.blas_instance_id];

          float3 local_normal =
              tri_shading_data[rec.tri_surface_id].interpolate_normal(rec.u,
                                                                      rec.v);
          rec.set_face_normal(
              r, normalize(to_float3(transpose(blas_instance.inv_transform) *
                                     float4(local_normal.x, local_normal.y,
                                            local_normal.z, 0.f))));
          float2 uv = tri_shading_data[rec.tri_surface_id].interpolate_uvs(
              rec.u, rec.v);

          rec.p = to_float3(blas_instance.transform *
                            float4(rec.p.x, rec.p.y, rec.p.z, 1.f));

          bool ray_scattered = false;
          Ray r_out;
          MaterialHandle mat_handle = blas_instance.material_handle;
          float3 material_attenuation = float3(0.f);

          switch (mat_handle.material_type) {
          case MATERIAL_LAMBERT:
            lambert_materials[mat_handle.material_index].scatter_ray(
                seed, rec, uv, material_attenuation, r_out,
                reinterpret_cast<cudaTextureObject_t *>(
                    constant_data.albedo_textures_buffer));
            ray_scattered = true;
            break;

          case MATERIAL_METALLIC:
            metal_materials[mat_handle.material_index].scatter_ray(
                seed, r, rec, material_attenuation, r_out);
            ray_scattered = true;
            break;

          case MATERIAL_DIELECTRIC:
            dielectric_materials[mat_handle.material_index].scatter_ray(
                seed, r, rec, material_attenuation, r_out);
            ray_scattered = true;
            break;

          case MATERIAL_EMISSIVE:
            emissive_materials[mat_handle.material_index].scatter_ray(emission);
            ray_scattered = false;
            break;
          }

          sample_radiance += attenuation * emission;

          if (!ray_scattered)
            break;

          attenuation *= material_attenuation;
          r = r_out;

        } else {
          float3 unit_direction = normalize(r.direction);
          float a = 0.5f * (unit_direction.y + 1.f);
          float3 sky =
              lerp(float3(0.7f, 0.7f, 0.7f), float3(0.5f, 0.7f, 1.f), a);

          sample_radiance += attenuation * sky;
          break;
        }
      }
      radiance += sample_radiance;
    }
  }

  // Average samples
  radiance *= constant_data.pixel_sample_scale;

  float4 *raw = reinterpret_cast<float4 *>(raw_buf);
  int idx = row * constant_data.image_width + col;

  float4 prev_color4 = raw[idx];
  float3 prev_color = float3(prev_color4.x, prev_color4.y, prev_color4.z);
  float3 accumulated = (prev_color * constant_data.frame_index + radiance) /
                       (constant_data.frame_index + 1);

  raw[idx] = float4(accumulated.x, accumulated.y, accumulated.z, 1.f);
}

void launch_trace_world(PushConstant &constant_data, cudaStream_t cu_stream,
                        void *output_buffer) {
  // dim3 block(32, 32);
  dim3 block(8, 8);
  dim3 grid((constant_data.image_width + block.x - 1) / block.x,
            (constant_data.image_height + block.y - 1) / block.y);
  trace_world<<<grid, block, 0, cu_stream>>>(output_buffer, constant_data);
}
