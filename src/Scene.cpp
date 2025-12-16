#include "Scene.hpp"
#include "Renderer.hpp"
// Vendor
#include <filesystem>
#include <Vendor/simdjson/simdjson.h>
#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>

struct Vertex {
  Vec3 position;
  Vec2 texcoord;
};

void load_default_scene(Renderer* renderer) {
  HASSERT(renderer);
  // Create world
  MaterialHandle mat_ground =
      renderer->add_lambert_material(Color(0.5f, 0.5f, 0.5f));
  renderer->add_sphere(Point3(0.f, -1000.f, 0.f), 1000.f, mat_ground);

  i32 size = 11;
  for (i32 a = -size; a < size; ++a) {
    for (i32 b = -size; b < size; ++b) {
      real choose_mat = random_real();
      Point3 center(a + 0.9f * random_real(), 0.2f, b + 0.9f * random_real());

      if ((center - Point3(4.f, 0.2f, 0.f)).length() > 0.9f) {
        MaterialHandle sphere_material;

        if (choose_mat < 0.8f) {
          // diffuse
          Color albedo = Color::random() * Color::random();
          sphere_material = renderer->add_lambert_material(albedo);
          renderer->add_sphere(center, 0.2f, sphere_material);
        } else if (choose_mat < 0.95f) {
          // metal
          Color albedo = Color::random(0.5f, 1.f);
          real fuzz = random_real(0.f, 0.5f);
          sphere_material = renderer->add_metal_material(albedo, fuzz);
          renderer->add_sphere(center, 0.2f, sphere_material);
        } else {
          // glass
          sphere_material = renderer->add_dielectric_material(1.5f);
          renderer->add_sphere(center, 0.2f, sphere_material);
        }
      }
    }
  }

  MaterialHandle mat1 = renderer->add_dielectric_material(1.5f);
  renderer->add_sphere(Point3(0.f, 1.f, 0.f), 1.f, mat1);
  MaterialHandle mat2 = renderer->add_lambert_material(Color(0.4f, 0.2f, 0.1f));
  renderer->add_sphere(Point3(-4.f, 1.f, 0.f), 1.f, mat2);
  MaterialHandle mat3 =
      renderer->add_metal_material(Color(0.7f, 0.6f, 0.5f), 0.f);
  renderer->add_sphere(Point3(4.f, 1.f, 0.f), 1.f, mat3);

  // Initialize
  renderer->center = Point3(13.f, 2.f, 3.f);
  renderer->lookat = Point3(0.f, 0.f, 0.f);
  renderer->vup = Vec3(0.f, 1.f, 0.f);
  renderer->defocus_angle = 0.6f;
  renderer->focus_dist = 10.f;
  renderer->init(384, 16.f / 9.f, 150, 10, 20.f);
}

bool load_scene(std::string scene_name, Renderer* renderer) {
  if (!std::filesystem::exists(scene_name)) {
    HERROR("Error: JSON file not found at path: {}", scene_name.c_str());
    return false;
	}
  using namespace simdjson;
  ondemand::parser parser;
  padded_string json = padded_string::load(scene_name);
  ondemand::document scene = parser.iterate(json);

  // Load camera settings
  ondemand::object camera = scene["camera"];
  ondemand::array center = camera["center"];
  int index = 0;
	for (auto value : center) {
    renderer->center.e[index++] = value.get_double().value();
	}
  index = 0;

  ondemand::array lookat = camera["lookat"];
	for (auto value : lookat) {
    renderer->lookat.e[index++] = value.get_double().value();
	}
  index = 0;

  ondemand::array vup = camera["vup"];
	for (auto value : vup) {
    renderer->vup.e[index++] = value.get_double().value();
	}
  index = 0;

  renderer->defocus_angle = camera["defocus_angle"].get_double().value();
  renderer->focus_dist = camera["focus_dist"].get_double().value();

  int screen_width = static_cast<int>(camera["screen_width"].get_int64().value());
  real aspect_ratio = camera["aspect_ratio"].get_double().value();
  int samples_per_pixel = camera["samples_per_pixel"].get_int64().value();
  int max_depth = camera["max_depth"].get_int64().value();
  real vfov_deg = camera["vfov_deg"].get_double().value();

  // Load Materials
  ondemand::array materials = scene["materials"];
  std::vector<MaterialHandle> material_handles(materials.count_elements());
  for (ondemand::object mat : materials) {
		int type_id = int64_t(mat["type_id"]);

    switch (type_id)
    {
    case MATERIAL_LAMBERT: {
      auto albedo_element = mat["albedo"];

      if (albedo_element.type() == ondemand::json_type::string) {
        std::string_view image_name = albedo_element.get_string();
        material_handles[index++] = 
          renderer->add_lambert_material(HOME_PATH "/Scenes/" + std::string(image_name));
      }
      else {
				ondemand::array albedo = mat["albedo"];
				std::vector<real> rgb;
				for (auto v : albedo) rgb.push_back(v.get_double().value());
				material_handles[index++] = renderer->add_lambert_material(Color(rgb[0], rgb[1], rgb[2]));
      }

      break;
    }
    case MATERIAL_METAL: {
			ondemand::array albedo = mat["albedo"];
			std::vector<real> rgb;
			for (auto v : albedo) rgb.push_back(v.get_double().value());
			real fuzz = mat["fuzz"].get_double().value();
			material_handles[index++] = renderer->add_metal_material(Color(rgb[0], rgb[1], rgb[2]), fuzz);
      break;
    }
    case MATERIAL_DIELECTRIC: {
			real ior = mat["ior"].get_double().value();
      material_handles[index++] = renderer->add_dielectric_material(ior);
      break;
    }
    default:
      HASSERT(false);
      break;
    }
	}
  index = 0;

  // Load Spheres
  ondemand::array spheres = scene["spheres"];
  for (ondemand::object sphere : spheres) {
		int mat_index = int64_t(sphere["material_index"]);
		double radius = double(sphere["radius"]);

		ondemand::array position = sphere["center"];
		std::vector<real> val;
		for (auto p : position) val.push_back(p.get_double().value());

		renderer->add_sphere(Point3(val[0], val[1], val[2]), radius, material_handles[mat_index]);
	}

  // Load Vertices
  std::vector<Vertex> vertices;
  ondemand::array vertices_ = scene["vertices"];
  for (ondemand::object vertex : vertices_) {
    Vertex v;
		ondemand::array pos = vertex["position"];
    size_t i = 0;
    for (auto p : pos) {
		  v.position.e[i++] = p.get_double().value();
    }
    i = 0;

		ondemand::array texcoord = vertex["texcoord"];
    for (auto p : texcoord) {
		  v.texcoord.e[i++] = p.get_double().value();
    }
    i = 0;
    vertices.push_back(v);
	}

  // Load Triangles
	size_t i = 0;
  u32 indices[3] = {};
  ondemand::array triangles = scene["triangles"];
  for (ondemand::object trig : triangles) {
    i = 0;
		i32 mat_index = int64_t(trig["material_index"]);
		ondemand::array indices_ = trig["indices"];
    for (auto index : indices_) {
		  indices[i++] = index.get_int64().value();
    }

    Vertex v0 = vertices[indices[0]];
    Vertex v1 = vertices[indices[1]];
    Vertex v2 = vertices[indices[2]];
    renderer->add_triangle(v0.position, v1.position, v2.position,
                           v0.texcoord, v1.texcoord, v2.texcoord,
                           material_handles[mat_index]);
	}


  renderer->init(screen_width, aspect_ratio, samples_per_pixel, max_depth, vfov_deg);

  return true;
}
