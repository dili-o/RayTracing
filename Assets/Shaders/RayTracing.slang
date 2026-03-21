#pragma once
#include "Triangle.slang"
#include "Random.slang"
#include "Material.slang"

struct UniformData {
  float3 pixel00_loc; float pad_0;
  float3 pixel_delta_u; float pad_1;
  float3 pixel_delta_v; float pad_2;
  float3 camera_center; float pad_3;
  TriangleGeom *triangle_geom_buffer;
  TriangleShading *triangle_shading_buffer;
  MaterialHandle *triangle_mat_ids_buffer;
  LambertMaterial *lambert_materials_buffer;
  MetalMaterial *metal_materials_buffer;
  DielectricMaterial *dielectric_materials_buffer;
};

struct PushConstants {
  UniformData *uniform_data_buffer;
  uint image_width;
  uint image_height;
  uint triangle_count;
  uint frame_index;
};

static uint max_depth = 10;

[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

[shader("compute")]
[numthreads(32,32,1)]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID,
                 uniform PushConstants pc) {
    int2 pixel_coord  = int2(dispatch_thread_id.xy);

    if (pixel_coord.x < pc.image_width && pixel_coord.y < pc.image_height) {
      uint seed = pixel_coord.x * 1973u 
                  ^ pixel_coord.y * 9277u 
                  ^ pc.frame_index * 26699u;

      float2 jitter = float2(rand(seed), rand(seed));
      float3 pixel_sample = pc.uniform_data_buffer[0].pixel00_loc.xyz +
                          ((pixel_coord.x + jitter.x) * pc.uniform_data_buffer[0].pixel_delta_u.xyz) +
                          ((pixel_coord.y + jitter.y) * pc.uniform_data_buffer[0].pixel_delta_v.xyz);
      float3 ray_direction = normalize(pixel_sample - pc.uniform_data_buffer[0].camera_center);
      Ray r = Ray(pc.uniform_data_buffer[0].camera_center, ray_direction);
      float3 attenuation = float3(1.f);

      for (uint d = 0; d < max_depth; ++d) {
        Interval ray_t = Interval(0.0001f, 1000.f);
        HitRecord rec;

        bool hit_anything = false;
        for (uint i = 0; i < pc.triangle_count; ++i) {
          if (pc.uniform_data_buffer[0].triangle_geom_buffer[i].hit(r, ray_t, rec)) {
            ray_t.max = rec.t;
            rec.tri_id = i;
            hit_anything = true;
          }
        }

        if (hit_anything) {
          // Set the HitRecord normal to the interpolated normal
          rec.normal = pc.uniform_data_buffer[0].triangle_shading_buffer[rec.tri_id]
                         .interpolate_normal(rec.u, rec.v);

          Ray r_out;
          MaterialHandle mat_handle = pc.uniform_data_buffer[0]
                                        .triangle_mat_ids_buffer[rec.tri_id];
          float3 material_attenuation = float3(0.f);
          switch (mat_handle.material_type) {
            case MATERIAL_LAMBERT: {
                pc.uniform_data_buffer[0]
                  .lambert_materials_buffer[mat_handle.material_index]
                  .scatter_ray(seed, rec, material_attenuation, r_out);
                  break;
              }
            case MATERIAL_METALLIC: {
                pc.uniform_data_buffer[0]
                  .metal_materials_buffer[mat_handle.material_index]
                  .scatter_ray(seed, r, rec, material_attenuation, r_out);
                  break;
              }
            default: break;
          }

          // float3 direction = rec.normal + rand_unit_vector(seed);  
          // r = Ray(rec.p + rec.normal * 0.001f, direction);
          r = r_out;
          attenuation *= material_attenuation;
        } else {
          float3 unit_direction = normalize(r.direction);
          float a = 0.5f * (unit_direction.y + 1.f);
          attenuation *= (1.f - a) * float3(1.f) + a * float3(0.5f, 0.7f, 1.f);
          break;
        }
      }

      float3 prev = output_image[pixel_coord].xyz;
      float3 accumulated = (prev * pc.frame_index + attenuation) / (pc.frame_index + 1);
      output_image[pixel_coord] = float4(accumulated, 1.f);
    }
}
