#include "Defines.hpp"
#include "Renderer.hpp"
#include "Assert.hpp"
#include "Material.hpp"


MaterialHandle RendererCPU::add_lambert_material(const Vec3 &albedo) {
  lambert_mats.push_back(std::make_shared<Lambertian>(Lambertian(albedo)));
  return {MATERIAL_LAMBERT, ((u32)lambert_mats.size() - 1)};
}

MaterialHandle RendererCPU::add_lambert_material(const std::string& filename) {
  lambert_mats.push_back(std::make_shared<Lambertian>(Lambertian(filename)));
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

void RendererCPU::add_triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
														const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
														Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
														MaterialHandle mat_handle) {
  std::shared_ptr<Material> mat = get_material(mat_handle);
  triangles.emplace_back(Triangle(v0, v1, v2, n0, n1, n2, uv_0, uv_1, uv_2, mat));
  Vec3 centroid = (v0 + v1 + v2) * 0.3333f;
  tri_centroids.push_back(centroid);
  tri_ids.push_back(static_cast<u32>(tri_ids.size()));
}

void RendererCPU::init(u32 image_width_, real aspect_ratio_,
                       u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) {
  initialize_camera(image_width_, aspect_ratio_, samples_per_pixel_, max_depth_,
                    vfov_deg_);
  u32 bvh_depth = 0;
  bvh[0] = BVH(triangles.data(), triangles.size(), false, tri_ids, tri_centroids, bvh_depth);
  bvh[0].set_transform(Mat4::translate(Vec3(-2.f, 1.f, 0.f)) *
                       Mat4::rotate_z(degrees_to_radians(-90.f)));

  bvh[1] = BVH(triangles.data(), triangles.size(), false, tri_ids, tri_centroids, bvh_depth);

  bvh[1].set_transform(Mat4::translate(Vec3(2.f, 1.f, 0.f)) *
                       Mat4::rotate_z(degrees_to_radians(90.f)));

  bvh[2] = BVH(triangles.data(), triangles.size(), false, tri_ids, tri_centroids, bvh_depth);
  bvh[2].set_transform(Mat4::identity());

  tlas = TLAS(bvh, ArraySize(bvh));
  tlas.build();
  show_image = true;
}

RendererCPU::~RendererCPU() {
}

#define TILE_W 4u
#define TILE_H 4u

void RendererCPU::render(u8 *out_pixels) {
  Interval intensity(0.f, 0.999f);

  for (u32 j = 0; j < image_height; j += TILE_H) {
    std::clog << "\rTiles remaining: " <<
                 ((image_height - j) / TILE_H) << ' ' << std::flush;
    u32 tile_h = std::min(TILE_H, image_height - j);
    for (u32 i = 0; i < image_width; i += TILE_W) {
			u32 tile_w = std::min(TILE_W, image_width  - i);
      for (u32 v = 0; v < tile_h; ++v) for (u32 u = 0; u < tile_w; ++u) {
				Color pixel_color = Color(0.f, 0.f, 0.f);

				for (u32 sample = 0; sample < samples_per_pixel; ++sample) {
					Ray ray = get_ray(i + u, j + v);
					pixel_color += ray_color(ray, max_depth);
				}
				pixel_color *= pixel_samples_scale;

				// Write the color
				real r = pixel_color.x;
				real g = pixel_color.y;
				real b = pixel_color.z;

				r = linear_to_gamma(r);
				g = linear_to_gamma(g);
				b = linear_to_gamma(b);

				i32 ir = i32(256 * intensity.clamp(r));
				i32 ig = i32(256 * intensity.clamp(g));
				i32 ib = i32(256 * intensity.clamp(b));

        u32 index = ((i + u) + ((j + v) * image_width)) * 3;
				out_pixels[index++] = ir;
				out_pixels[index++] = ig;
				out_pixels[index] = ib;
      }
    }
  }
  std::clog << "\rDone.                 \n";
}

Color RendererCPU::ray_color(const Ray &r, u32 depth) {
  if (depth <= 0) {
    return Color(0.f, 0.f, 0.f);
  }
  HitRecord rec;
  rec.t = infinity;
  Interval ray_t = Interval(0.001f, infinity);
  bool hit_anything = tlas.intersect(r, ray_t, rec);

  if (hit_anything) {
		// Use the interpolated normal to set the outward normal

		const Triangle &trig = triangles[rec.tri_id];
		f32 trig_u = rec.u;
		f32 trig_v = rec.v;
    const Vec3 &n0 = trig.n0;
    const Vec3 &n1 = trig.n1;
    const Vec3 &n2 = trig.n2;
		const f32 alpha = 1.f - trig_u - trig_v;
		Vec3 interpolated_normal = unit_vector(alpha * n0 + trig_u * n1 + trig_v * n2);
		rec.normal = interpolated_normal;

    // Get UV
		const Vec2 &uv_0 = trig.uv_0;
		const Vec2 &uv_1 = trig.uv_1;
		const Vec2 &uv_2 = trig.uv_2;
    rec.u = alpha * uv_0.x + trig_u * uv_1.x + trig_v * uv_2.x;
    rec.v = alpha * uv_0.y + trig_u * uv_1.y + trig_v * uv_2.y;

    rec.mat = trig.mat;

    Ray scattered;
    Color attenuation;
    if (rec.mat->scatter_ray(r, rec, attenuation, scattered)) {
      return attenuation * ray_color(scattered, depth - 1);
    }
    return Color(0.f, 0.f, 0.f);
  }

  Vec3 unit_direction = unit_vector(r.direction);
  real a = 0.5f * (unit_direction.y + 1.f);
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
  Vec3 pixel_sample = pixel00_loc + ((i + offset.x) * pixel_delta_u) +
                      ((j + offset.y) * pixel_delta_v);

  Vec3 ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
  Vec3 ray_direction = pixel_sample - ray_origin;

  return Ray(ray_origin, ray_direction);
}

std::shared_ptr<Material> RendererCPU::get_material(MaterialHandle mat_handle) {
  switch (mat_handle.type) {
  case MATERIAL_LAMBERT: {
    return lambert_mats[mat_handle.index];
  }
  case MATERIAL_METAL: {
    return metal_mats[mat_handle.index];
  }
  case MATERIAL_DIELECTRIC: {
    return dielectric_mats[mat_handle.index];
  }
  default: {
    HASSERT_MSG(false, "Invalid material type given");
  }
  }
}



