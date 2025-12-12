#pragma once
#include "HitRecord.slang"
#include "Random.slang"

static const uint MATERIAL_LAMBERT  = 0;
static const uint MATERIAL_METALLIC = 1;
static const uint MATERIAL_DIELECTRIC = 2;

#ifndef ALBEDO_TEXTURE_COUNT
#define ALBEDO_TEXTURE_COUNT 1
#error "ALBEDO_TEXTURE_COUNT must be defined"
#endif

[[vk::binding(2, 0)]]
Sampler2D albedo_textures[ALBEDO_TEXTURE_COUNT];

struct MaterialLambert {
  uint image_index; // rgb only
};

struct MaterialMetal {
  float4 albedo_fuzz; // rbg and fuzz
};

struct MaterialDielectric {
  float refraction_index;
};

void scatter_ray_lambert(inout uint seed, in MaterialLambert mat,
                            in HitRecord rec, out float3 attenuation,
                            out Ray r_out) {
  float3 scattered_direction = rec.normal + rand_unit_vector(seed);
  if (near_zero(scattered_direction)) {
    scattered_direction = rec.normal;
  }

  r_out = Ray(rec.p, scattered_direction);
  attenuation = albedo_textures[NonUniformResourceIndex(mat.image_index)]
                  .Sample(float2(rec.u, rec.v)).xyz;
}

#define NORMALIZE_REFLECTION

void scatter_ray_metal(inout uint seed, in Ray r_in, in MaterialMetal mat,
                            in HitRecord rec, out float3 attenuation,
                       out Ray r_out) {
  float3 reflected = reflect(r_in.direction, rec.normal);

#ifdef NORMALIZE_REFLECTION
  reflected += (mat.albedo_fuzz.w * rand_unit_vector(seed));
  r_out = Ray(rec.p, reflected);
  attenuation = mat.albedo_fuzz.xyz;
#else
  reflected = normalize(reflected) + 
              (mat.albedo_fuzz.w * rand_unit_vector(seed));
  r_out = Ray(rec.p, reflected);
  if (dot(r_out.direction, rec.normal) > 0)
    attenuation = mat.albedo_fuzz.xyz;
#endif // NORMALIZE_REFLECTION
}

static float reflectance(float cosine, float refraction_index) {
  // Use Schlick's approximation for reflectance.
  float r0 = (1 - refraction_index) / (1 + refraction_index);
  r0 = r0 * r0;
  return r0 + (1 - r0)* pow((1 - cosine), 5);
}

inline float3 rtiow_refract(const float3 uv, const float3 n, float etai_over_etat) {
    float cos_theta = fmin(dot(-uv, n), 1.0);
    float3 r_out_perp =  etai_over_etat * (uv + cos_theta*n);
    float3 r_out_parallel = -sqrt(fabs(1.0 - dot(r_out_perp, r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
}

void scatter_ray_dielectric(inout uint seed, in Ray r_in, in MaterialDielectric mat,
                            in HitRecord rec, out float3 attenuation,
                            out Ray r_out) {
  attenuation = float3(1.f);
  float ri = rec.front_face ? (1.f / mat.refraction_index) : mat.refraction_index;

  float3 unit_direction = normalize(r_in.direction);
  float cos_theta = fmin(dot(-unit_direction, rec.normal), 1.f);
  float sin_theta = sqrt(fmax(0.f, 1.f - cos_theta * cos_theta));

  bool cannot_refract = ri * sin_theta > 1.f;
  float3 direction;

  if (cannot_refract || reflectance(cos_theta, ri) > rand(seed))
    direction = reflect(unit_direction, rec.normal);
  else
    direction = rtiow_refract(unit_direction, rec.normal, ri);

  r_out = Ray(rec.p, direction);
}

