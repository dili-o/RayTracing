#ifndef CAMERA_H
#define CAMERA_H

#include "Hittable.hpp"

class Camera {
public:
  void render(const Hittable &world, u8 *out_pixels) {
    Interval intensity(0.f, 0.999f);
    u32 index = 0;
    for (int j = 0; j < image_height; j++) {
      std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                << std::flush;
      for (int i = 0; i < image_width; i++) {
        Color pixel_color = Color(0.f, 0.f, 0.f);

        for (u32 sample = 0; sample < samples_per_pixel; ++sample) {
          Ray ray = get_ray(i, j);
          pixel_color += ray_color(ray, world);
        }
        pixel_color *= pixel_samples_scale;

        // Write the color
        real r = pixel_color.x();
        real g = pixel_color.y();
        real b = pixel_color.z();

        i32 ir = i32(256 * intensity.clamp(r));
        i32 ig = i32(256 * intensity.clamp(g));
        i32 ib = i32(256 * intensity.clamp(b));

        out_pixels[index++] = ir;
        out_pixels[index++] = ig;
        out_pixels[index++] = ib;
      }
    }
  }

  void initialize(u32 image_width_, real aspect_ratio_,
                  u32 samples_per_pixel_) {
    image_width = image_width_;
    aspect_ratio = aspect_ratio_;
    samples_per_pixel = samples_per_pixel_;
    image_height = u32(image_width / aspect_ratio);
    image_height = (image_height < 1) ? 1 : image_height;

    center = Point3(0.f, 0.f, 0.f);
    pixel_samples_scale = 1.f / samples_per_pixel;

    // Determine viewport dimensions.
    real focal_length = 1.f;
    real viewport_height = 2.f;
    real viewport_width = viewport_height * (real(image_width) / image_height);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    Vec3 viewport_u = Vec3(viewport_width, 0.f, 0.f);
    Vec3 viewport_v = Vec3(0.f, -viewport_height, 0.f);

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    pixel_delta_u = viewport_u / (real)image_width;
    pixel_delta_v = viewport_v / (real)image_height;

    // Calculate the location of the upper left pixel.
    Vec3 viewport_upper_left = center - Vec3(0.f, 0.f, focal_length) -
                               viewport_u / 2.f - viewport_v / 2.f;
    pixel00_loc = viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v);
  }

public:
  real aspect_ratio;
  u32 image_width;
  u32 image_height;
  Point3 center;      // Camera center
  Point3 pixel00_loc; // Location of pixel 0, 0
  Vec3 pixel_delta_u; // Offset to pixel to the right
  Vec3 pixel_delta_v; // Offset to pixel below
  u32 samples_per_pixel;
  real pixel_samples_scale;

private:
  Color ray_color(const Ray &r, const Hittable &world) const {
    HitRecord rec;
    if (world.hit(r, Interval(0.f, infinity), rec)) {
      return 0.5f * (rec.normal + Color(1.f, 1.f, 1.f));
    }

    Vec3 unit_direction = unit_vector(r.direction());
    real a = 0.5f * (unit_direction.y() + 1.f);
    return (1.f - a) * Color(1.f, 1.f, 1.f) + a * Color(0.5f, 0.7f, 1.f);
  }

  Vec3 sample_square() {
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
    // square.
    return Vec3(random_real() - 0.5f, random_real() - 0.5f, 0.f);
  }

  Ray get_ray(i32 i, i32 j) {
    // Construct a camera ray originating from the origin and directed at
    // randomly sampled point around the pixel location i, j.

    Vec3 offset = sample_square();
    Vec3 pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) +
                        ((j + offset.y()) * pixel_delta_v);

    Vec3 ray_origin = center;
    Vec3 ray_direction = pixel_sample - ray_origin;

    return Ray(ray_origin, ray_direction);
  }
};

#endif
