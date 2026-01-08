#include "Random.slang"
#include "Material.slang"
#include "TLAS.slang"

struct ShaderPushConstant {
  float4 pixel00_loc;   
  float4 pixel_delta_u; 
  float4 pixel_delta_v; 
  float4 camera_center; // 64

  uint image_width;
  uint image_height;
  uint seed_value;
  uint samples_per_pixel; // 80

  float pixel_samples_scale;
  uint max_depth;
  uint triangle_count;
  float defocus_angle; // 96

  float4 defocus_disk_u;
  float4 defocus_disk_v; // 128
};

struct Uniforms{
	Triangle *triangles;
	uint *tri_ids;
	TLASNode *tlas_nodes;
	BVH *bvhs;
	BVHNode *bvh_nodes;
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
      float3 ray_color = float3(0.f);
      uint seed = pixel_coord.x + pixel_coord.y * pc.seed_value;

      for (uint sample = 0; sample < pc.samples_per_pixel; ++sample) {
        float2 jitter = float2(rand(seed), rand(seed));

        float3 pixel_sample = pc.pixel00_loc.xyz +
                            ((pixel_coord.x + jitter.x) * pc.pixel_delta_u.xyz) +
                            ((pixel_coord.y + jitter.y) * pc.pixel_delta_v.xyz);
        float3 ray_origin = (pc.defocus_angle <= 0.f) ? pc.camera_center.xyz : 
                            defocus_disk_sample(seed, pc.camera_center.xyz,
                            pc.defocus_disk_u.xyz, pc.defocus_disk_v.xyz);
        float3 ray_direction = pixel_sample - ray_origin;

        Ray r = Ray(ray_origin, ray_direction);
        float3 attenuation = float3(1.f);
        for (uint d = 0; d < pc.max_depth; ++d) {
					Interval ray_t = Interval(0.001f, 1e30f);
					HitRecord rec;
					rec.t = 1e30f;

					bool hit = intersect_tlas(r, ray_t, rec,
							uniform_buffer[0].tlas_nodes, 
							uniform_buffer[0].bvhs, 
							uniform_buffer[0].bvh_nodes, 
							uniform_buffer[0].triangles,
						  uniform_buffer[0].tri_ids);
          if (hit) {
						// Use the interpolated normal to set the outward normal
						Triangle trig = uniform_buffer[0].triangles[rec.tri_id];
						float trig_u = rec.u;
						float trig_v = rec.v;
						const float3 n0 = trig.n0.xyz;
						const float3 n1 = trig.n1.xyz;
						const float3 n2 = trig.n2.xyz;

						const float alpha = 1.f - trig_u - trig_v;
						float3 interpolated_normal = normalize(alpha * n0 + trig_u * n1 + trig_v * n2);
						rec.normal = interpolated_normal;

						// Get UV
						const float2 uv_0 = trig.uv_0;
						const float2 uv_1 = trig.uv_1;
						const float2 uv_2 = trig.uv_2;
						rec.u = alpha * uv_0.x + trig_u * uv_1.x + trig_v * uv_2.x;
						rec.v = alpha * uv_0.y + trig_u * uv_1.y + trig_v * uv_2.y;

						rec.material_index = trig.material_index;
						rec.material_type = trig.material_type;

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

      output_image[pixel_coord] += float4(ray_color.r, ray_color.g, ray_color.b, 1.f);
    }
}
