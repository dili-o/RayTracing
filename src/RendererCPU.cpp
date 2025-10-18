#include "Assert.hpp"
#include "Material.hpp"
#include "Renderer.hpp"

MaterialHandle RendererCPU::add_lambert_material(const Vec3 &albedo) {
  lambert_mats.push_back(std::make_shared<Lambertian>(Lambertian(albedo)));
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle RendererCPU::add_metal_material(const Vec3 &albedo,
                                               real fuzziness) {
  metal_mats.push_back(std::make_shared<Metal>(Metal(albedo, fuzziness)));
  return {MATERIAL_METAL, ((u32)metal_mats.size() - 1)};
}

MaterialHandle RendererCPU::add_dielectric_material(real refraction_index) {
  dielectric_mats.push_back(
      std::make_shared<Dielectric>(Dielectric(refraction_index)));
  return {MATERIAL_DIELECTRIC, ((u32)dielectric_mats.size() - 1)};
}

void RendererCPU::add_sphere(const Vec3 &origin, real radius,
                             MaterialHandle mat_handle) {
  std::shared_ptr<Material> mat;
  switch (mat_handle.type) {
  case MATERIAL_LAMBERT: {
    mat = lambert_mats[mat_handle.index];
    break;
  }
  case MATERIAL_METAL: {
    mat = metal_mats[mat_handle.index];
    break;
  }
  case MATERIAL_DIELECTRIC: {
    mat = dielectric_mats[mat_handle.index];
    break;
  }
  default: {
    HASSERT_MSG(false, "Invalid material type given");
    break;
  }
  }

  world.add(make_shared<Sphere>(origin, radius, mat));
}

void RendererCPU::init(u32 image_width_, real aspect_ratio_,
                       u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) {
  show_image = true;
  initialize_camera(image_width_, aspect_ratio_, samples_per_pixel_, max_depth_,
                    vfov_deg_);
}

void RendererCPU::render(u8 *out_pixels) {
  Interval intensity(0.f, 0.999f);
  u32 index = 0;
  for (u32 j = 0; j < image_height; j++) {
    std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
              << std::flush;
    for (u32 i = 0; i < image_width; i++) {
      Color pixel_color = Color(0.f, 0.f, 0.f);

      for (u32 sample = 0; sample < samples_per_pixel; ++sample) {
        Ray ray = get_ray(i, j);
        pixel_color += ray_color(ray, max_depth, world);
      }
      pixel_color *= pixel_samples_scale;

      // Write the color
      real r = pixel_color.x();
      real g = pixel_color.y();
      real b = pixel_color.z();

      r = linear_to_gamma(r);
      g = linear_to_gamma(g);
      b = linear_to_gamma(b);

      i32 ir = i32(256 * intensity.clamp(r));
      i32 ig = i32(256 * intensity.clamp(g));
      i32 ib = i32(256 * intensity.clamp(b));

      out_pixels[index++] = ir;
      out_pixels[index++] = ig;
      out_pixels[index++] = ib;
    }
  }
  std::clog << "\rDone.                 \n";
}

Color RendererCPU::ray_color(const Ray &r, u32 depth,
                             const Hittable &world) const {
  if (depth <= 0) {
    return Color(0.f, 0.f, 0.f);
  }
  HitRecord rec;
  if (world.hit(r, Interval(0.001f, infinity), rec)) {
    Ray scattered;
    Color attenuation;
    if (rec.mat->scatter_ray(r, rec, attenuation, scattered)) {
      return attenuation * ray_color(scattered, depth - 1, world);
    }
    return Color(0.f, 0.f, 0.f);
  }

  Vec3 unit_direction = unit_vector(r.direction());
  real a = 0.5f * (unit_direction.y() + 1.f);
  return (1.f - a) * Color(1.f, 1.f, 1.f) + a * Color(0.5f, 0.7f, 1.f);
}

Vec3 RendererCPU::sample_square() const {
  // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
  // square.
  return Vec3(random_real() - 0.5f, random_real() - 0.5f, 0.f);
}

Point3 RendererCPU::defocus_disk_sample() const {
  // Returns a random point in the camera defocus disk.
  Vec3 p = random_in_unit_disk();
  return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
}

Ray RendererCPU::get_ray(i32 i, i32 j) const {
  // Construct a camera ray originating from the defocus disk and directed at
  // a randomly sampled point around the pixel location i, j.

  Vec3 offset = sample_square();
  Vec3 pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) +
                      ((j + offset.y()) * pixel_delta_v);

  Vec3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
  Vec3 ray_direction = pixel_sample - ray_origin;

  return Ray(ray_origin, ray_direction);
}
