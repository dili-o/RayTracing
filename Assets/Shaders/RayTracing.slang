#pragma once
#include "BVHNode.slang"
#include "Material.slang"
#include "TLAS.slang"

struct UniformData {
  float3 pixel00_loc;
  float pad_0;
  float3 pixel_delta_u;
  float pad_1;
  float3 pixel_delta_v;
  float pad_2;
  float3 camera_center;
  float pad_3;

  TriangleGeom *triangle_geom_buffer;
  TriangleShading *triangle_shading_buffer;
  TLASNode *tlas_nodes_buffer;
  BVHNode *bvh_nodes_buffer;
  BLAS *blas_buffer;
  BLASInstance *blas_instances_buffer;
  uint *tri_ids_buffer;
  LambertMaterial *lambert_materials_buffer;
  MetalMaterial *metal_materials_buffer;
  DielectricMaterial *dielectric_materials_buffer;
  EmissiveMaterial *emissive_materials_buffer;
};

struct PushConstants {
  UniformData *uniform_data_buffer;
  uint image_width;
  uint image_height;
  uint triangle_count;
  uint frame_index;
};

static const uint max_depth = 10;
static const uint samples_per_pixel = 3;
static const float pixel_sample_scale = 1.f / float(samples_per_pixel);

[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

[shader("compute")]
[numthreads(32, 32, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID,
                  uniform PushConstants pc) {
  int2 pixel_coord = int2(dispatch_thread_id.xy);

  if (pixel_coord.x < pc.image_width && pixel_coord.y < pc.image_height) {
    UniformData data = pc.uniform_data_buffer[0];

    uint seed =
        pixel_coord.x * 1973u ^ pixel_coord.y * 9277u ^ pc.frame_index * 26699u;

    float3 radiance = float3(0.f);

    for (uint s = 0; s < samples_per_pixel; ++s) {
      uint sample_seed = seed + s * 9781u; // decorrelate samples

      float2 jitter = float2(rand(sample_seed), rand(sample_seed));

      float3 pixel_sample =
          data.pixel00_loc.xyz +
          ((pixel_coord.x + jitter.x) * data.pixel_delta_u.xyz) +
          ((pixel_coord.y + jitter.y) * data.pixel_delta_v.xyz);

      float3 ray_direction = normalize(pixel_sample - data.camera_center);
      Ray r = Ray(data.camera_center, ray_direction);

      float3 attenuation = float3(1.f);
      float3 sample_radiance = float3(0.f);

      for (uint d = 0; d < max_depth; ++d) {
        Interval ray_t = Interval(0.0001f, 1000.f);
        HitRecord rec;
        rec.t = 1000.f;
        float3 emission = float3(0.f);

        bool hit_anything = intersect_tlas(
            r, ray_t, rec, data.tlas_nodes_buffer, data.blas_instances_buffer,
            data.blas_buffer, data.bvh_nodes_buffer, data.triangle_geom_buffer,
            data.tri_ids_buffer);

        if (hit_anything) {
          BLASInstance blas_instance =
              data.blas_instances_buffer[rec.blas_instance_id];

          float3 local_normal =
              data.triangle_shading_buffer[rec.tri_id].interpolate_normal(
                  rec.u, rec.v);

          rec.set_face_normal(
              r, normalize(mul(transpose(blas_instance.inv_transform),
                               float4(local_normal, 0.f))
                               .xyz));

          rec.p = mul(blas_instance.transform, float4(rec.p, 1.f)).xyz;

          bool ray_scattered = false;
          Ray r_out;
          MaterialHandle mat_handle = blas_instance.material_handle;
          float3 material_attenuation = float3(0.f);

          switch (mat_handle.material_type) {
          case MATERIAL_LAMBERT:
            data.lambert_materials_buffer[mat_handle.material_index]
                .scatter_ray(sample_seed, rec, material_attenuation, r_out);
            ray_scattered = true;
            break;

          case MATERIAL_METALLIC:
            data.metal_materials_buffer[mat_handle.material_index].scatter_ray(
                sample_seed, r, rec, material_attenuation, r_out);
            ray_scattered = true;
            break;

          case MATERIAL_DIELECTRIC:
            data.dielectric_materials_buffer[mat_handle.material_index]
                .scatter_ray(sample_seed, r, rec, material_attenuation, r_out);
            ray_scattered = true;
            break;

          case MATERIAL_EMISSIVE:
            data.emissive_materials_buffer[mat_handle.material_index]
                .scatter_ray(emission);
            ray_scattered = false;
            break;
          }

          sample_radiance += attenuation * emission;

          if (!ray_scattered)
            break;

          attenuation *= material_attenuation;
          r = r_out;

        } else {
          float3 sky = float3(0.f);
          sample_radiance += attenuation * sky;
          break;
        }
      }

      radiance += sample_radiance;
    }

    // Average samples
    radiance *= pixel_sample_scale;

    float3 prev = output_image[pixel_coord].xyz;
    float3 accumulated =
        (prev * pc.frame_index + radiance) / (pc.frame_index + 1);
    output_image[pixel_coord] = float4(accumulated, 1.f);
  }
}
