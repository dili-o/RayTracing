#include "Random.slang"
#include "World.slang"
#include "Material.slang"

struct ShaderPushConstant {
  float4 pixel00_loc;   // 16 
  float4 pixel_delta_u; // 16
  float4 pixel_delta_v; // 16
  float4 camera_center; // 16

  uint image_width;
  uint image_height;
  uint sphere_count;
  uint samples_per_pixel;

  float pixel_samples_scale;
  uint max_depth;
  float padding[2];
};

struct Uniforms{
  Sphere *spheres;
  MaterialLambert *lambert_mats;
  MaterialMetal *metal_mats;
  MaterialDielectric *dielectric_mats;
};

[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

[[vk::binding(1, 0)]]
StructuredBuffer<Uniforms> uniform_buffer;

[shader("compute")]
[numthreads(8,8,1)]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID,
                 uniform ShaderPushConstant pc) {
    int2 pixel_coord  = int2(dispatch_thread_id.xy);

    if (pixel_coord.x < pc.image_width && pixel_coord.y < pc.image_height) {
      Interval ray_t = Interval(0.001f, 1000.f);
      float3 ray_color = float3(0.f);
      uint seed = pixel_coord.x + pixel_coord.y * 1973u;

      for (uint sample = 0; sample < pc.samples_per_pixel; ++sample) {
        float2 jitter = float2(rand(seed), rand(seed));

        float3 pixel_sample = pc.pixel00_loc.xyz +
                            ((pixel_coord.x + jitter.x) * pc.pixel_delta_u.xyz) +
                            ((pixel_coord.y + jitter.y) * pc.pixel_delta_v.xyz);
        float3 ray_direction = pixel_sample - pc.camera_center.xyz;

        Ray r = Ray(pc.camera_center.xyz, ray_direction);
        float3 attenuation = float3(1.f);
        for (uint d = 0; d < pc.max_depth; ++d) {
          HitRecord rec;
          // Test ray against all spheres
          if (hit_world(r, ray_t, rec, uniform_buffer[0].spheres, pc.sphere_count)) {
            Ray scattered;
            float3 material_attenuation = float3(0.f);

            switch (rec.material_type) {
            case MATERIAL_LAMBERT: {
                scatter_ray_lambert(seed, uniform_buffer[0].lambert_mats[rec.material_index],
                    rec, material_attenuation, scattered);
                break;
            }
            case MATERIAL_METALLIC: {
                scatter_ray_metal(seed, r, uniform_buffer[0].metal_mats[rec.material_index],
                                  rec, material_attenuation, scattered);
                break;
            }
            case MATERIAL_DIELECTRIC: {
                scatter_ray_dielectric(seed, r, uniform_buffer[0].dielectric_mats[rec.material_index],
                                       rec, material_attenuation, scattered);
                break;
            }
            default: break;
            }

            r = scattered;
            attenuation *= material_attenuation;
          } else {
            float3 unit_direction = normalize(r.direction);
            float a = 0.5f * (unit_direction.y + 1.f);
            attenuation *= (1.f - a) * float3(1.f, 1.f, 1.f) + a * float3(0.5f, 0.7f, 1.f);
            break;
          }
        }
        ray_color += attenuation;
      }

      ray_color *= pc.pixel_samples_scale;

      output_image[pixel_coord] = float4(ray_color.r, ray_color.g, ray_color.b, 1.f);
    }
}
