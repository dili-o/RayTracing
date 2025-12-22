#include "Renderer.hpp"
#include "Assert.hpp"
#include "Material.hpp"

bool intersect_aabb(const Ray& ray, const Vec3 &bmin, const Vec3 &bmax, const f32 t) {
	f32 tx1 = (bmin.x - ray.origin.x) / ray.direction.x, tx2 = (bmax.x - ray.origin.x) / ray.direction.x;
	f32 tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
	f32 ty1 = (bmin.y - ray.origin.y) / ray.direction.y, ty2 = (bmax.y - ray.origin.y) / ray.direction.y;
	tmin = std::max(tmin, std::min(ty1, ty2)), tmax = std::min(tmax, std::max(ty1, ty2));
	f32 tz1 = (bmin.z - ray.origin.z) / ray.direction.z, tz2 = (bmax.z - ray.origin.z) / ray.direction.z;
	tmin = std::max(tmin, std::min(tz1, tz2)), tmax = std::min(tmax, std::max(tz1, tz2));
	return tmax >= tmin && tmin < t && tmax > 0;
}

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

void RendererCPU::add_sphere(const Vec3 &origin, real radius,
                             MaterialHandle mat_handle) {
  std::shared_ptr<Material> mat = get_material(mat_handle);
  world.add(make_shared<Sphere>(origin, radius, mat));
}

void RendererCPU::add_triangle(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
														const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
														Vec2 uv_0, Vec2 uv_1, Vec2 uv_2,
														MaterialHandle mat_handle) {
  std::shared_ptr<Material> mat = get_material(mat_handle);
  triangles.emplace_back(Triangle(v0, v1, v2, n0, n1, n2, uv_0, uv_1, uv_2, mat));
  Vec3 centroid = (v0 + v1 + v2) * 0.3333f;
  tri_centroids.push_back(centroid);
}

void RendererCPU::init(u32 image_width_, real aspect_ratio_,
                       u32 samples_per_pixel_, u32 max_depth_, real vfov_deg_) {
  show_image = true;
  initialize_camera(image_width_, aspect_ratio_, samples_per_pixel_, max_depth_,
                    vfov_deg_);
  build_bvh();
}

RendererCPU::~RendererCPU() {
}

void RendererCPU::render(u8 *out_pixels) {
  Interval intensity(0.f, 0.999f);
  u32 index = 0;

  for (u32 j = 0; j < image_height; j++) {
    std::clog << "\rScanlines remaining: " <<
                 (image_height - j) << ' ' << std::flush;
    for (u32 i = 0; i < image_width; i++) {
      Color pixel_color = Color(0.f, 0.f, 0.f);

      for (u32 sample = 0; sample < samples_per_pixel; ++sample) {
        Ray ray = get_ray(i, j);
        pixel_color += ray_color(ray, max_depth, world);
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

      out_pixels[index++] = ir;
      out_pixels[index++] = ig;
      out_pixels[index++] = ib;
    }
  }
  std::clog << "\rDone.                 \n";
}

bool RendererCPU::intersect_bvh(const Ray& ray, const u32 node_idx,
                                const Interval &ray_t, HitRecord &rec) const {
	const BVHNode& node = bvh_nodes[node_idx];
	if (!intersect_aabb(ray, node.aabb_min, node.aabb_max, ray_t.max)) return false;
	bool hit_anything = false;
	if (node.is_leaf()) {
    f32 closest_so_far = ray_t.max;
    for (u32 i = 0; i < node.prim_count; ++i) {
      if (triangles[node.left_first + i].hit(ray, Interval(ray_t.min, closest_so_far), rec)) {
				hit_anything = true;
				closest_so_far = rec.t;
      }
    }
	}
	else {
		f32 closest_so_far = ray_t.max;
		
		if (intersect_bvh(ray, node.left_first, Interval(ray_t.min, closest_so_far), rec)) {
			hit_anything = true;
			closest_so_far = rec.t;
		}
		
		if (intersect_bvh(ray, node.left_first + 1, Interval(ray_t.min, closest_so_far), rec)) {
			hit_anything = true;
			closest_so_far = rec.t;
		}
	}
  
  return hit_anything;
}

Color RendererCPU::ray_color(const Ray &r, u32 depth,
                             const Hittable &world) const {
  if (depth <= 0) {
    return Color(0.f, 0.f, 0.f);
  }
  HitRecord rec;
  bool hit_anything = false;
  const Interval ray_t = Interval(0.001f, infinity);
	//real closest_so_far = ray_t.max;
 // for (const Triangle& tri : triangles) {
	//	if (tri.hit(r, Interval(ray_t.min, closest_so_far), rec)) {
	//		hit_anything = true;
	//		closest_so_far = rec.t;
	//	}
 // }
  hit_anything = intersect_bvh(r, 0, ray_t, rec);
  if (hit_anything) {
    Ray scattered;
    Color attenuation;
    if (rec.mat->scatter_ray(r, rec, attenuation, scattered)) {
      return attenuation * ray_color(scattered, depth - 1, world);
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

void RendererCPU::build_bvh() {
	std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();

  const size_t N = triangles.size();
  bvh_nodes.resize(N * 2 - 1);
  u32 nodes_used = 1;

  BVHNode& root = bvh_nodes[0];
	root.left_first = 0;
	root.prim_count = N;
	update_node_bounds(0);
	// subdivide recursively
	subdivide_node(0, nodes_used);

  auto end = std::chrono::high_resolution_clock::now();
  f64 seconds = std::chrono::duration<f64>(end - start).count();
  std::cout << "BVH build time: " << seconds << " seconds\n";
}

void RendererCPU::update_node_bounds(u32 node_idx) {
  BVHNode& node = bvh_nodes[node_idx];
	node.aabb_min = Vec3(infinity);
	node.aabb_max = Vec3(-infinity);
	for (u32 i = 0; i < node.prim_count; ++i) {
		Triangle& leaf_tri = triangles[node.left_first + i];
		node.aabb_min = Vec3::fmin(node.aabb_min, leaf_tri.v0);
		node.aabb_min = Vec3::fmin(node.aabb_min, leaf_tri.v1);
		node.aabb_min = Vec3::fmin(node.aabb_min, leaf_tri.v2);
		node.aabb_max = Vec3::fmax(node.aabb_max, leaf_tri.v0);
		node.aabb_max = Vec3::fmax(node.aabb_max, leaf_tri.v1);
		node.aabb_max = Vec3::fmax(node.aabb_max, leaf_tri.v2);
	}
}

void RendererCPU::subdivide_node(u32 node_idx, u32 &nodes_used) {
  BVHNode& node = bvh_nodes[node_idx];
  if (node.prim_count <= 2) return;
  Vec3 extents = node.aabb_max - node.aabb_min;
  i32 axis = 0;
  if (extents.y > extents.x) axis = 1;
  if (extents.z > extents[axis]) axis = 2;
  f32 split_pos = node.aabb_min[axis] + extents[axis] * 0.5f;

  // Partition triangles
  i32 i = node.left_first;
  i32 j = i + node.prim_count - 1;
  while (i <= j) {
    if (tri_centroids[i][axis] < split_pos) {
			i++;
    }
    else {
      std::swap(tri_centroids[i], tri_centroids[j]);
			std::swap(triangles[i], triangles[j--]);
    }
  }
  // Abort split if one of the sides is empty
	i32 left_count = i - node.left_first;
	if (left_count == 0 || left_count == node.prim_count) return;
	// Create child nodes
	int left_child_idx = nodes_used++;
	int rightChildIdx = nodes_used++;
	bvh_nodes[left_child_idx].left_first = node.left_first;
	bvh_nodes[left_child_idx].prim_count = left_count;
	bvh_nodes[rightChildIdx].left_first = i;
	bvh_nodes[rightChildIdx].prim_count = node.prim_count - left_count;
	node.left_first = left_child_idx;
	node.prim_count = 0;
	update_node_bounds(left_child_idx);
	update_node_bounds(rightChildIdx);
	// recurse
	subdivide_node(left_child_idx, nodes_used);
	subdivide_node(rightChildIdx, nodes_used);
}
