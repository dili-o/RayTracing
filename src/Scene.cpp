#include "Scene.hpp"
#include "Renderer.hpp"
// Vendor
#include <Vendor/simdjson/simdjson.h>
#define STB_IMAGE_IMPLEMENTATION
#include <Vendor/stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace fs = std::filesystem;

struct Vertex {
  Vec3 position;
  Vec3 normal;
  Vec2 texcoord;

  bool operator==(const Vertex& other) const {
		return position == other.position && normal == other.normal && texcoord == other.texcoord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
      return ((hash<Vec3>()(vertex.position) ^
                   (hash<Vec3>()(vertex.normal) << 1)) >> 1) ^
                   (hash<Vec2>()(vertex.texcoord) << 1);;
		}
	};
}

void load_default_scene(Renderer* renderer) {
  HASSERT(renderer);
  // TODO:
}

bool load_scene(const fs::path &scene_path, Renderer* renderer) {
  if (!fs::exists(scene_path)) {
    HERROR("Error: JSON file not found at path: {}", scene_path.string().c_str());
    return false;
  }
	fs::path scene_parent_path = fs::path(scene_path).parent_path();
  using namespace simdjson;
  ondemand::parser parser;
  padded_string json = padded_string::load(scene_path.string());
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

  // Load default Material
  MaterialHandle default_material = renderer->add_lambert_material(Color(0.8f, 0.0f, 0.8f));

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
          renderer->add_lambert_material(scene_parent_path.string() + "/" + std::string(image_name));
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

  // Load Vertices
	std::vector<Vertex> vertices;
  bool has_vertices_field = scene["vertices"].error() != NO_SUCH_FIELD;
  if (has_vertices_field) {
		ondemand::array vertices_ = scene["vertices"];
		for (ondemand::object vertex : vertices_) {
			size_t i = 0;
			Vertex v;
			ondemand::array pos = vertex["position"];
			for (auto p : pos) {
				v.position.e[i++] = p.get_double().value();
			}
			i = 0;

			ondemand::array normal = vertex["normal"];
			for (auto n : normal) {
				v.normal.e[i++] = n.get_double().value();
			}
			i = 0;

			ondemand::array texcoord = vertex["texcoord"];
			for (auto p : texcoord) {
				v.texcoord.e[i++] = p.get_double().value();
			}
			i = 0;
			vertices.push_back(v);
		}
  }

  // Load Triangles
  auto has_triangles_field = scene["triangles"].error() != NO_SUCH_FIELD;
  if (has_triangles_field) {
    ondemand::array triangles = scene["triangles"];
    for (ondemand::object trig : triangles) {
      u32 indices[3] = {};
      size_t i = 0;
      i32 mat_index = int64_t(trig["material_index"]);
      ondemand::array indices_ = trig["indices"];
      for (auto index : indices_) {
        indices[i++] = index.get_int64().value();
      }

      const Vertex& v0 = vertices[indices[0]];
      const Vertex& v1 = vertices[indices[1]];
      const Vertex& v2 = vertices[indices[2]];
      renderer->add_triangle(v0.position, v1.position, v2.position,
				v0.normal, v1.normal, v2.normal,
        v0.texcoord, v1.texcoord, v2.texcoord,
        material_handles[mat_index]);
    }
  }

  // Load model
  auto model_field = scene["model"];
  if (model_field.error() != NO_SUCH_FIELD) {
		ondemand::object model = scene["model"];
		std::string model_path = scene_parent_path.string() + "/" + std::string(model["path"].get_string().value());
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> obj_shapes;
		std::vector<tinyobj::material_t> obj_materials;
		std::string warn;
		std::string err;

    fs::path old_dir = fs::current_path();
    fs::path model_parent_path = fs::path(model_path).parent_path();
		fs::current_path(model_parent_path);

		if (!tinyobj::LoadObj(&attrib, &obj_shapes, &obj_materials, &warn, &err, model_path.c_str())) {
			throw std::runtime_error(err);
		}
    if (!warn.empty()) {
      HWARN("tinyobj: {}", warn.c_str());
    }
    
    // Load model materials
    std::vector<MaterialHandle> model_mats;
    model_mats.resize(obj_materials.size());
    for (size_t i = 0; i < obj_materials.size(); ++i) {
      const auto &mat = obj_materials[i];
      if (!mat.diffuse_texname.empty()) {
				std::string img_path = model_parent_path.string() + "/" +
           mat.diffuse_texname;
				model_mats[i] = renderer->add_lambert_material(img_path);
      } else {
        model_mats[i] = renderer->add_lambert_material(Color(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]));
      }
    }
		fs::current_path(old_dir);

		std::vector<u32> obj_indices;
		std::unordered_map<Vertex, u32> uniqueVertices{};

		for (const auto& shape : obj_shapes) {
      const auto &mat_ids = shape.mesh.material_ids;
      size_t old_indices_size = obj_indices.size();
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

        // Tex Coords
				if (index.texcoord_index == -1) {
					vertex.texcoord = { 0.f, 0.f };
				}
				else {
					vertex.texcoord = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
					};
				}
        
        // Normals
        if (index.normal_index != -1) {
          vertex.normal = {
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2]
          };
        } else {
          HASSERT(false);
        }

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}

				obj_indices.push_back(uniqueVertices[vertex]);
			}

			for (size_t idx = old_indices_size, mat_id = 0; idx < obj_indices.size(); idx += 3, ++mat_id) {
				const Vertex &v0 = vertices[obj_indices[idx]];
				const Vertex &v1 = vertices[obj_indices[idx + 1]];
				const Vertex &v2 = vertices[obj_indices[idx + 2]];
        MaterialHandle material; 
        if (model_mats.size()) {
          material = mat_ids[mat_id] == -1 ? default_material
                                           : model_mats[mat_ids[mat_id]];
        } else {
          material = default_material;
        }
				renderer->add_triangle(v0.position, v1.position, v2.position,
														 v0.normal, v1.normal, v2.normal,
															 v0.texcoord, v1.texcoord, v2.texcoord,
															 material);
			}
		}
  }

  renderer->init(screen_width, aspect_ratio, samples_per_pixel, max_depth, vfov_deg);

  return true;
}
