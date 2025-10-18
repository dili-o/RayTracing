#pragma once
#include "HittableList.hpp"
#include "Material.hpp"
#include "Sphere.hpp"

class Renderer {
public:
  virtual void render(u8 *out_pixels) = 0;
  virtual MaterialHandle add_lambert_material(const Vec3 &albedo) = 0;
  virtual MaterialHandle add_metal_material(const Vec3 &albedo,
                                            real fuzziness) = 0;
  virtual MaterialHandle add_dielectric_material(real refraction_index) = 0;
  virtual void add_sphere(const Vec3 &origin, real radius,
                          MaterialHandle mat) = 0;

  virtual void init(u32 image_width_, real aspect_ratio_,
                    u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) = 0;

public:
  real aspect_ratio;
  u32 max_depth;
  u32 image_width;
  u32 image_height;
  Point3 center;      // camera center
  Point3 pixel00_loc; // Location of pixel 0, 0
  Vec3 pixel_delta_u; // Offset to pixel to the right
  Vec3 pixel_delta_v; // Offset to pixel below
  u32 samples_per_pixel;
  real pixel_samples_scale;
  real vfov;
  Point3 lookat = Point3(0.f, 0.f, -1.f); // Point camera is looking at
  Vec3 vup = Vec3(0.f, 1.f, 0.f);         // camera-relative "up" direction
  Vec3 u, v, w;                           // camera frame basis vectors

  real defocus_angle = 0; // Variation angle of rays through each pixel
  real focus_dist =
      10; // Distance from camera lookfrom point to plane of perfect focus
  Vec3 defocus_disk_u; // Defocus disk horizontal radius
  Vec3 defocus_disk_v; // Defocus disk vertical radius
  bool show_image;     // Open the image after rendering
protected:
  void initialize_camera(u32 image_width_, real aspect_ratio_,
                         u32 samples_per_pixel_, u32 max_depth_,
                         real vfov_deg_) {
    image_width = image_width_;
    aspect_ratio = aspect_ratio_;
    samples_per_pixel = samples_per_pixel_;
    image_height = u32(image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;
    max_depth = max_depth_;
    vfov = vfov_deg_;

    pixel_samples_scale = 1.f / samples_per_pixel;

    // Determine viewport dimensions.
    real theta = degrees_to_radians(vfov);
    real h = std::tan(theta / 2.f);
    real viewport_height = 2.f * h * focus_dist;
    real viewport_width = viewport_height * (real(image_width) / image_height);

    // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
    w = unit_vector(center - lookat);
    u = unit_vector(cross(vup, w));
    v = cross(w, u);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    Vec3 viewport_u =
        viewport_width * u; // Vector across viewport horizontal edge
    Vec3 viewport_v =
        viewport_height * -v; // Vector down viewport vertical edge

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    pixel_delta_u = viewport_u / (real)image_width;
    pixel_delta_v = viewport_v / (real)image_height;

    // Calculate the location of the upper left pixel.
    Vec3 viewport_upper_left =
        center - (focus_dist * w) - viewport_u / 2.f - viewport_v / 2.f;
    pixel00_loc = viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    real defocus_radius =
        focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.f));
    defocus_disk_u = u * defocus_radius;
    defocus_disk_v = v * defocus_radius;
  }
};

class RendererCPU final : public Renderer {
public:
  void init(u32 image_width_, real aspect_ratio_, u32 samples_per_pixel_,
            u32 max_depth_, real vfov_deg_) override;

  void render(u8 *out_pixels) override;
  MaterialHandle add_lambert_material(const Vec3 &albedo) override;
  MaterialHandle add_metal_material(const Vec3 &albedo,
                                    real fuzziness) override;
  MaterialHandle add_dielectric_material(real refraction_index) override;
  void add_sphere(const Vec3 &origin, real radius, MaterialHandle mat) override;

private:
  Color ray_color(const Ray &r, u32 depth, const Hittable &world) const;
  Vec3 sample_square() const;
  Point3 defocus_disk_sample() const;
  Ray get_ray(i32 i, i32 j) const;

private:
  HittableList world;
  std::vector<std::shared_ptr<Lambertian>> lambert_mats;
  std::vector<std::shared_ptr<Metal>> metal_mats;
  std::vector<std::shared_ptr<Dielectric>> dielectric_mats;
};

class RendererVk final : public Renderer {
public:
  ~RendererVk();

  void init(u32 image_width_, real aspect_ratio_, u32 samples_per_pixel_,
            u32 max_depth_, real vfov_deg_) override;

  void render(u8 *out_pixels) override;
  MaterialHandle add_lambert_material(const Vec3 &albedo) override;
  MaterialHandle add_metal_material(const Vec3 &albedo,
                                    real fuzziness) override;
  MaterialHandle add_dielectric_material(real refraction_index) override;
  void add_sphere(const Vec3 &origin, real radius, MaterialHandle mat) override;

private:
  std::vector<GpuLambert> lambert_mats;
  std::vector<GpuMetal> metal_mats;
  std::vector<GpuDielectric> dielectric_mats;
  std::vector<SpherePacked> spheres;
};
