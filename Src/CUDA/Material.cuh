#pragma once
#include "HitRecord.cuh"
#include "Random.cuh"

static const uint32_t MATERIAL_LAMBERT = 0;
static const uint32_t MATERIAL_METALLIC = 1;
static const uint32_t MATERIAL_DIELECTRIC = 2;
static const uint32_t MATERIAL_EMISSIVE = 3;

struct MaterialHandle {
  uint32_t material_index;
  uint32_t material_type;
};

struct LambertMaterial {
  __device__ void scatter_ray(uint32_t &seed, HitRecord &rec,
                              const float2 &tex_coord, float3 &attenuation,
                              Ray &r_out,
                              cudaTextureObject_t *albedo_textures) {
    float3 scattered_direction = rec.normal + rand_unit_vector(seed);
    if (near_zero(scattered_direction)) {
      scattered_direction = rec.normal;
    }

    r_out = Ray(rec.p + (scattered_direction * 0.0001f), scattered_direction);
    float4 sampled =
        tex2D<float4>(albedo_textures[index], tex_coord.x, tex_coord.y);

    attenuation = float3(sampled.x, sampled.y, sampled.z);
  }

  uint32_t index;
};

#define NORMALIZE_REFLECTION
struct alignas(16) MetalMaterial {
  __device__ void scatter_ray(uint32_t &seed, const Ray &r_in,
                              const HitRecord &rec, float3 &attenuation,
                              Ray &r_out) {
    float3 reflected = reflect(r_in.direction, rec.normal);

#ifdef NORMALIZE_REFLECTION
    reflected += (fuzz * rand_unit_vector(seed));
    r_out = Ray(rec.p, reflected);
    attenuation = albedo;
#else
    reflected = normalize(reflected) + (fuzz * rand_unit_vector(seed));
    r_out = Ray(rec.p, reflected);
    if (dot(r_out.direction, rec.normal) > 0)
      attenuation = albedo;
#endif // NORMALIZE_REFLECTION
  }

  float3 albedo;
  float fuzz;
};

__device__ float reflectance(float cosine, float refraction_index) {
  // Use Schlick's approximation for reflectance.
  float r0 = (1 - refraction_index) / (1 + refraction_index);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}

__device__ inline float3 rtiow_refract(float3 uv, const float3 &n,
                                       float etai_over_etat) {
  float cos_theta = fmin(dot(-uv, n), 1.f);
  float3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
  float3 r_out_parallel = -sqrt(fabs(1.f - dot(r_out_perp, r_out_perp))) * n;
  return r_out_perp + r_out_parallel;
}

struct DielectricMaterial {
  __device__ void scatter_ray(uint32_t &seed, const Ray &r_in,
                              const HitRecord &rec, float3 &attenuation,
                              Ray &r_out) {
    attenuation = float3(1.f);
    float ri = rec.front_face ? (1.f / refraction_index) : refraction_index;

    float3 unit_direction = normalize(r_in.direction);
    float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.f);
    float sin_theta = sqrt(fmax(0.f, 1.f - cos_theta * cos_theta));

    bool cannot_refract = ri * sin_theta > 1.f;
    float3 direction;

    if (cannot_refract || reflectance(cos_theta, ri) > rand_float(seed))
      direction = reflect(unit_direction, rec.normal);
    else
      direction = rtiow_refract(unit_direction, rec.normal, ri);

    r_out = Ray(rec.p, direction);
  }

  float refraction_index;
};

struct alignas(16) EmissiveMaterial {
  __device__ void scatter_ray(float3 &emission) { emission = intensity; }

  float3 intensity;
  float padding;
};
