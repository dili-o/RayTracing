#ifndef MATERIAL_H
#define MATERIAL_H

#include "Hittable.hpp"
#include "Textures.hpp"

class Material {
public:
  virtual ~Material() = default;

  virtual bool scatter_ray(const Ray &r_in, const HitRecord &rec,
                           Color &attenuation, Ray &r_out) const {
    return false;
  }
};

class Lambertian : public Material {
public:
  Lambertian(const Color &albedo) 
    : albedo(std::make_shared<SolidTexture>(albedo)) {}
  Lambertian(const std::string &filename) 
    : albedo(std::make_shared<ImageTexture>(filename)) {}

  bool scatter_ray(const Ray &r_in, const HitRecord &rec, Color &attenuation,
                   Ray &r_out) const override {
    Vec3 scatter_direction = rec.normal + random_unit_vector();
    if (scatter_direction.near_zero()) {
      scatter_direction = rec.normal;
    }

    r_out = Ray(rec.p, scatter_direction);
    attenuation = albedo->sample(rec.u, rec.v);
    return true;
  }

private:
  std::shared_ptr<Texture> albedo;
};

class Metal : public Material {
public:
  Metal(const Color &albedo, real fuzz)
      : albedo(albedo), fuzz(fuzz < 1.f ? fuzz : 1.f) {}

  bool scatter_ray(const Ray &r_in, const HitRecord &rec, Color &attenuation,
                   Ray &r_out) const override {
    Vec3 reflected = reflect(r_in.direction(), rec.normal);
    reflected = unit_vector(reflected) + (fuzz * random_unit_vector());
    r_out = Ray(rec.p, reflected);
    attenuation = albedo;
    return (dot(r_out.direction(), rec.normal) > 0);
  }

private:
  Color albedo;
  real fuzz;
};

class Dielectric : public Material {
public:
  Dielectric(real refraction_index) : refraction_index(refraction_index) {}

  bool scatter_ray(const Ray &r_in, const HitRecord &rec, Color &attenuation,
                   Ray &r_out) const override {
    attenuation = Color(1.f, 1.f, 1.f);
    real ri = rec.front_face ? (1.f / refraction_index) : refraction_index;

    Vec3 unit_direction = unit_vector(r_in.direction());
    real cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.f);
    real sin_theta = std::sqrt(1.f - cos_theta * cos_theta);

    bool cannot_refract = ri * sin_theta > 1.f;
    Vec3 direction;

    if (cannot_refract || reflectance(cos_theta, ri) > random_real())
      direction = reflect(unit_direction, rec.normal);
    else
      direction = refract(unit_direction, rec.normal, ri);

    r_out = Ray(rec.p, direction);
    return true;
  }

private:
  // Refractive index in vacuum or air, or the ratio of the material's
  // refractive index over the refractive index of the enclosing media
  real refraction_index;

  static real reflectance(real cosine, real refraction_index) {
    // Use Schlick's approximation for reflectance.
    real r0 = (1.f - refraction_index) / (1.f + refraction_index);
    r0 = r0 * r0;
    return r0 + (1.f - r0) * std::pow((1 - cosine), 5.f);
  }
};

enum MaterialType { MATERIAL_LAMBERT, MATERIAL_METAL, MATERIAL_DIELECTRIC };

struct GpuLambert {
  u32 image_index;
};

struct alignas(16) GpuMetal {
  f32 albedo_fuzz[4];
};

struct GpuDielectric {
  f32 refraction_index;
};

struct MaterialHandle {
  MaterialType type;
  u32 index;
};

#endif
