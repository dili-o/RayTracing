#include "SceneGraph.hpp"
#include "Core/Assert.hpp"
#include "Core/RingQueue.hpp"
#include "Material.hpp"
#include "Platform/FileIO.h"
#include "Renderer.hpp"
#include "Transform.hpp"
// Vendor
#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <imgui/imgui.h>
#include <numeric>
#include <stb_image.h>

namespace fs = std::filesystem;

namespace hlx {
static void gltf_set_vec3(glm::vec3 &glm_vec,
                          const fastgltf::math::fvec3 &f_vec) {
  glm_vec.x = f_vec.data()[0];
  glm_vec.y = f_vec.data()[1];
  glm_vec.z = f_vec.data()[2];
}

static void gltf_set_quat(glm::quat &glm_vec,
                          const fastgltf::math::fquat &f_vec) {
  glm_vec.x = f_vec.data()[0];
  glm_vec.y = f_vec.data()[1];
  glm_vec.z = f_vec.data()[2];
  glm_vec.w = f_vec.data()[3];
}

static glm::mat4 gltf_get_matrix4x4(const fastgltf::math::fmat4x4 &m) {
  return glm::mat4(m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1],
                   m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3],
                   m[3][0], m[3][1], m[3][2], m[3][3]);
}

static MaterialHandle gltf_load_texture(Renderer *renderer,
                                        fastgltf::Asset &asset,
                                        fastgltf::Texture &texture,
                                        std::string_view texture_path) {
  MaterialHandle material_handle;
  fastgltf::Image &image = asset.images[texture.imageIndex.value()];

  std::visit(
      fastgltf::visitor{
          [&](auto &arg) {
            HERROR("Current image type: {} is not yet supported",
                   typeid(arg).name());
          },
          [&](fastgltf::sources::Array &array) {
            i32 texture_width, texture_height;
            i32 channel_count;

            if (!stbi_info_from_memory(
                    reinterpret_cast<stbi_uc *>(array.bytes.data()),
                    array.bytes.size(), &texture_width, &texture_height,
                    &channel_count)) {
              HERROR("gltf_load_pbr_texture::stbi_info_from_memory::fastgltf::"
                     "sources::Array failed to "
                     "load texture info, Reason: {}",
                     stbi_failure_reason());
            }
            u8 *pixels = stbi_load_from_memory(
                reinterpret_cast<stbi_uc *>(array.bytes.data()),
                array.bytes.size(), &texture_width, &texture_height,
                &channel_count, STBI_rgb_alpha);
            HASSERT(pixels);

            material_handle = renderer->add_lambert_material(
                texture_width, texture_height, pixels);
            stbi_image_free(pixels);
          },
          [&](fastgltf::sources::URI &filePath) {
            HASSERT_MSG(
                filePath.fileByteOffset == 0,
                "Gltf filePath.uri has a byteOffset"); // We don't support
                                                       // offsets with stbi.

            HASSERT_MSG(
                filePath.uri.isLocalPath(),
                "Gltf filePath.uri is not local"); // We're only capable of
                                                   // loading local files.

            material_handle = renderer->add_lambert_material(
                std::string(texture_path) + "\\" + filePath.uri.c_str());
          },
          [&](fastgltf::sources::BufferView &view) {
            auto &bufferView = asset.bufferViews[view.bufferViewIndex];
            auto &buffer = asset.buffers[bufferView.bufferIndex];

            std::visit(
                fastgltf::visitor{
                    // We only care about VectorWithMime here, because
                    // we specify LoadExternalBuffers, meaning all
                    // buffers are already loaded into a vector.
                    [](auto &arg) {
                      HERROR("Current image type: {} is not yet supported",
                             typeid(arg).name());
                    },
                    [&](fastgltf::sources::Array &array) {
                      i32 texture_width, texture_height;
                      i32 channel_count;

                      if (!stbi_info_from_memory(
                              reinterpret_cast<stbi_uc *>(
                                  array.bytes.data() + bufferView.byteOffset),
                              array.bytes.size(), &texture_width,
                              &texture_height, &channel_count)) {
                        HERROR("gltf_load_pbr_texture::stbi_info_from_"
                               "memoryfastgltf::sources::BufferView::Array "
                               "failed to "
                               "load texture info, Reason: {}",
                               stbi_failure_reason());
                      }
                      u8 *pixels = stbi_load_from_memory(
                          reinterpret_cast<u8 *>(array.bytes.data()) +
                              bufferView.byteOffset,
                          array.bytes.size(), &texture_width, &texture_height,
                          &channel_count, STBI_rgb_alpha);
                      HASSERT(pixels);

                      material_handle = renderer->add_lambert_material(
                          texture_width, texture_height, pixels);

                      stbi_image_free(pixels);
                    },
                    [&](fastgltf::sources::Vector &vector) {
                      i32 texture_width, texture_height;
                      i32 channel_count;

                      if (!stbi_info_from_memory(
                              reinterpret_cast<stbi_uc *>(
                                  vector.bytes.data() + bufferView.byteOffset),
                              bufferView.byteLength, &texture_width,
                              &texture_height, &channel_count)) {
                        HERROR("gltf_load_pbr_texture::stbi_info_from_"
                               "memory::fastgltf::sources::BufferView::Vector "
                               "failed to "
                               "load texture info, Reason: {}",
                               stbi_failure_reason());
                      }

                      u8 *pixels = stbi_load_from_memory(
                          reinterpret_cast<u8 *>(vector.bytes.data()) +
                              bufferView.byteOffset,
                          vector.bytes.size(), &texture_width, &texture_height,
                          &channel_count, STBI_rgb_alpha);
                      HASSERT(pixels);

                      material_handle = renderer->add_lambert_material(
                          texture_width, texture_height, pixels);

                      stbi_image_free(pixels);
                    }},
                buffer.data);
          },
      },
      image.data);

  return material_handle;
}

static bool load_gltf_scene(SceneGraph &scene_graph, Renderer *renderer,
                            std::string_view path, std::string_view file_name,
                            u32 parent_node) {
  fs::path cwd = fs::current_path();
  fs::current_path(path);

  fs::path std_path = fs::path(path) / file_name;

  // Parse the glTF file and get the constructed asset
  fastgltf::Parser parser(fastgltf::Extensions::None);

  constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember |
                                fastgltf::Options::AllowDouble |
                                fastgltf::Options::LoadExternalBuffers |
                                fastgltf::Options::GenerateMeshIndices;
  auto gltf_file = fastgltf::MappedGltfFile::FromPath(std_path);
  if (!bool(gltf_file)) {
    HERROR("Failed to open glTF file: {}",
           fastgltf::getErrorMessage(gltf_file.error()));
    return false;
  }

  fastgltf::Expected<fastgltf::Asset> asset =
      parser.loadGltf(gltf_file.get(), std_path.parent_path(), gltf_options);
  if (asset.error() != fastgltf::Error::None) {
    HERROR("Failed to open glTF file: {}",
           fastgltf::getErrorMessage(asset.error()));
    return false;
  }

  // Load materials
  std::vector<MaterialHandle> material_handles(asset->materials.size());
  for (size_t i = 0; i < asset->materials.size(); ++i) {
    fastgltf::Material &material = asset->materials[i];

    if (material.pbrData.baseColorTexture.has_value()) {
      material_handles[i] = gltf_load_texture(
          renderer, asset.get(),
          asset->textures[material.pbrData.baseColorTexture.value()
                              .textureIndex],
          path);
    } else {
      u8 def_colour[4];
      f32 *albedo_colour = material.pbrData.baseColorFactor.data();
      def_colour[0] =
          static_cast<u8>(std::clamp(albedo_colour[0], 0.0f, 1.0f) * 255.0f);
      def_colour[1] =
          static_cast<u8>(std::clamp(albedo_colour[1], 0.0f, 1.0f) * 255.0f);
      def_colour[2] =
          static_cast<u8>(std::clamp(albedo_colour[2], 0.0f, 1.0f) * 255.0f);
      def_colour[3] =
          static_cast<u8>(std::clamp(albedo_colour[3], 0.0f, 1.0f) * 255.0f);

      material_handles[i] = renderer->add_lambert_material(1, 1, def_colour);
    }
  }

  std::vector<i32> gltf_to_hierarchy_node(asset->nodes.size(), -1);

  RingQueue<size_t> gltf_node_queue{};
  gltf_node_queue.init(asset->nodes.size());

  // If we only have one root node then we add it to the scene graph's root,
  // else we create a new node to be the root of the gltf asset
  fastgltf::Scene &root_scene =
      asset
          ->scenes[asset->defaultScene.has_value() ? asset->defaultScene.value()
                                                   : 0];
  i32 root_node_parent = root_scene.nodeIndices.size() == 1
                             ? parent_node
                             : scene_graph.add_node(parent_node, std::string());

  // Enqueue root nodes
  for (u32 i = 0; i < root_scene.nodeIndices.size(); ++i) {
    gltf_node_queue.enqueue(root_scene.nodeIndices[i]);
    fastgltf::Node &node = asset->nodes[root_scene.nodeIndices[i]];
    std::string node_name = node.name.c_str();

    gltf_to_hierarchy_node[root_scene.nodeIndices[i]] =
        scene_graph.add_node(root_node_parent, node_name);
  }

  // Work through nodes
  while (gltf_node_queue.size) {
    size_t gltf_node_index = UINT32_MAX;
    gltf_node_queue.dequeue(&gltf_node_index);

    fastgltf::Node &node = asset->nodes[gltf_node_index];
    i32 node_hierarchy_index = gltf_to_hierarchy_node[gltf_node_index];
    // Transform
    if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
      const fastgltf::TRS &trs = std::get<fastgltf::TRS>(node.transform);

      Transform transform{};
      gltf_set_vec3(transform.position, trs.translation);
      gltf_set_quat(transform.rotation, trs.rotation);
      gltf_set_vec3(transform.scale, trs.scale);
      scene_graph.update_node_local_transform(node_hierarchy_index,
                                              transform.get_mat4());

    } else if (std::holds_alternative<fastgltf::math::fmat4x4>(
                   node.transform)) {
      const fastgltf::math::fmat4x4 &mat =
          std::get<fastgltf::math::fmat4x4>(node.transform);

      Transform transform{};
      glm::mat4 matrix = gltf_get_matrix4x4(mat);
      transform.set_transform(matrix);
      scene_graph.update_node_local_transform(node_hierarchy_index,
                                              transform.get_mat4());
    }

    // Add child nodes to the queue
    for (u32 i = 0; i < node.children.size(); ++i) {
      gltf_node_queue.enqueue(node.children[i]);
      fastgltf::Node &child_node = asset->nodes[node.children[i]];
      std::string child_node_name = child_node.name.c_str();
      gltf_to_hierarchy_node[node.children[i]] =
          scene_graph.add_node(node_hierarchy_index, child_node_name);
    }

    if (node.meshIndex.has_value()) {
      fastgltf::Mesh &gltf_mesh = asset->meshes[node.meshIndex.value()];

      for (u32 i = 0; i < gltf_mesh.primitives.size(); ++i) {
        // Single Vertex buffer for each mesh primitive
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> tex_coords;
        std::vector<u32> indices;
        fastgltf::Primitive &primitive = gltf_mesh.primitives[i];
        HASSERT_MSG(primitive.type == fastgltf::PrimitiveType::Triangles,
                    "Non-Triangle Primitive type");

        std::string primitive_name = "Mesh_" + std::to_string(i);

        // Only create a new hierarchy node if the gltf node has multiple
        // primitives
        i32 primitive_node =
            gltf_mesh.primitives.size() == 1
                ? node_hierarchy_index
                : scene_graph.add_node(node_hierarchy_index, primitive_name);

        // Index buffer per primitive
        {
          fastgltf::Accessor &index_accessor =
              asset->accessors[primitive.indicesAccessor.value()];

          indices.reserve(index_accessor.count);

          fastgltf::iterateAccessor<u32>(
              asset.get(), index_accessor,
              [&](std::uint32_t idx) { indices.push_back(idx); });
        }

        // load position vertices
        {
          fastgltf::Accessor &pos_accessor =
              asset->accessors[primitive.findAttribute("POSITION")
                                   ->accessorIndex];
          positions.reserve(pos_accessor.count);

          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
              asset.get(), pos_accessor,
              [&](fastgltf::math::fvec3 v, size_t index) {
                positions.push_back(
                    glm::vec3(v.data()[0], v.data()[1], v.data()[2]));
              });
        }

        // load normal vertices
        {
          auto normal_it = primitive.findAttribute("NORMAL");
          if (normal_it == primitive.attributes.end()) {
            normals.reserve(positions.size());
            for (size_t i = 0; i < indices.size(); i += 3) {
              glm::vec3 &p0 = positions[indices[i]];
              glm::vec3 &p1 = positions[indices[i + 1]];
              glm::vec3 &p2 = positions[indices[i + 2]];

              glm::vec3 edge1 = p1 - p0;
              glm::vec3 edge2 = p2 - p0;

              glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
              normals.push_back(normal);
              normals.push_back(normal);
              normals.push_back(normal);
            }
          } else {
            fastgltf::Accessor &normal_accessor =
                asset->accessors[normal_it->accessorIndex];

            normals.reserve(normal_accessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset.get(), normal_accessor,
                [&](fastgltf::math::fvec3 n, size_t index) {
                  normals.push_back(
                      glm::vec3(n.data()[0], n.data()[1], n.data()[2]));
                });
          }
        }

        // load tex_coord vertices
        {
          fastgltf::Attribute *tex_attribute =
              primitive.findAttribute("TEXCOORD_0");
          if (tex_attribute != primitive.attributes.end()) {
            fastgltf::Accessor &tex_coord_accessor =
                asset->accessors[tex_attribute->accessorIndex];

            tex_coords.reserve(tex_coord_accessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                asset.get(), tex_coord_accessor,
                [&](fastgltf::math::fvec2 uv, size_t index) {
                  tex_coords.push_back(glm::vec2(uv.data()[0], uv.data()[1]));
                });
          } else {
            tex_coords.resize(positions.size());
          }
        }
        u32 blas_id =
            renderer->add_blas(positions, normals, tex_coords, indices);

        // TODO: Right now just using the first lambert material
        MaterialHandle mat_handle =
            primitive.materialIndex.has_value()
                ? material_handles[primitive.materialIndex.value()]
                : renderer->default_material;
        scene_graph.set_node_blas_instance(
            primitive_node,
            renderer->add_blas_instance(blas_id, glm::mat4(1.f), mat_handle));
      }
    }
  }

  scene_graph.update_node_local_transform(root_node_parent, glm::mat4(1.f));

  gltf_node_queue.shutdown();
  fs::current_path(cwd);
  return true;
}

template <typename T, typename Index = u32>
void erase_selected(std::vector<T> &v, const std::vector<Index> &selection) {
  v.resize(std::distance(
      v.begin(),
      std::stable_partition(
          v.begin(), v.end(), [&selection, &v](const T &item) {
            return !std::binary_search(
                selection.begin(), selection.end(),
                static_cast<Index>(static_cast<const T *>(&item) - &v[0]));
          })));
}

void add_unique_idx(std::vector<uint32_t> &v, uint32_t index) {
  if (!std::binary_search(v.begin(), v.end(), index))
    v.push_back(index);
}

void SceneGraph::init(u32 max_node_capacity) {
  node_index_pool.init(max_node_capacity);
  nodes.resize(max_node_capacity);
  local_transforms.resize(max_node_capacity);
  global_transforms.resize(max_node_capacity);
  node_names.resize(max_node_capacity);
  add_node(INVALID_NODE_ID, "Root");
}

void SceneGraph::shutdown(Renderer *renderer) {
  if (node_index_pool.size) {
    node_index_pool.release_all();
    node_index_pool.shutdown();
  }

  for (const auto &[node_id, blas_instance_id] : node_to_blas_instance) {
    if (blas_instance_id != UINT32_MAX)
      renderer->remove_blas_instance(blas_instance_id);
  }
}

u32 SceneGraph::add_node(u32 parent, std::string name) {
  i32 level;
  if (parent != INVALID_NODE_ID) {
    level = nodes[parent].level + 1;
  } else {
    // Root node
    level = 0;
  }
  HASSERT_MSG(level < MAX_NODE_LEVEL,
              "SceneGraph::add_node(): level exceeds MAX_NODE_LEVEL");
  u32 node_id = node_index_pool.obtain_new();
  local_transforms[node_id] = glm::mat4(1.f);
  global_transforms[node_id] = glm::mat4(1.f);
  if (name.empty()) {
    node_names[node_id] = std::string("Node") + std::to_string(node_id);
  } else {
    node_names[node_id] = name;
  }
  nodes[node_id] = {.parent_node = parent, .level = level};

  SceneNode &node = nodes[node_id];
  if (parent != INVALID_NODE_ID) {
    SceneNode &parent_node = nodes[parent];
    const i32 first_child = parent_node.first_child;

    if (first_child == INVALID_NODE_ID) {
      // Parent has no children
      parent_node.first_child = node_id;
      node.last_sibling = node_id;
    } else {
      // Parent has children
      i32 last_sibling = nodes[first_child].last_sibling;
      nodes[last_sibling].next_sibling = node_id;
      nodes[first_child].last_sibling = node_id;
    }
  }

  node_to_blas_instance[node_id] = UINT32_MAX;

  return node_id;
}

void SceneGraph::set_node_blas_instance(u32 node_id, u32 blas_instance_id) {
  node_to_blas_instance[node_id] = blas_instance_id;
}

std::string_view SceneGraph::get_node_name(u32 node_id) const {
  if (node_id == INVALID_NODE_ID)
    return nullptr;
  return node_names[node_id];
}

void SceneGraph::queue_to_update(u32 node_id) {
  if (node_id == INVALID_NODE_ID)
    return;
  const i32 level = nodes[node_id].level;
  nodes_to_update[level].push_back(node_id);

  for (u32 id = nodes[node_id].first_child; id != INVALID_NODE_ID;
       id = nodes[id].next_sibling) {
    queue_to_update(id);
  }
}

void SceneGraph::collect_nodes_to_delete(u32 node_id,
                                         std::vector<u32> &node_indices) {
  // Add all children to the node_indices vector to be deleted
  for (u32 n = nodes[node_id].first_child; n != INVALID_NODE_ID;
       n = nodes[n].next_sibling) {
    add_unique_idx(node_indices, n);
    collect_nodes_to_delete(n, node_indices);
  }
}

void SceneGraph::delete_scene_nodes(const std::vector<u32> &nodes_to_delete) {
  auto indices_to_delete = nodes_to_delete;
  for (u32 i : indices_to_delete) {
    collect_nodes_to_delete(i, indices_to_delete);
  }

  std::vector<u32> nodes_copy(nodes.size());
  std::iota(nodes_copy.begin(), nodes_copy.end(), 0u);

  const size_t old_size = nodes_copy.size();
  erase_selected(nodes_copy, indices_to_delete);
  std::vector<u32> new_indices(old_size, INVALID_NODE_ID);
  for (u32 i = 0u; i < nodes_copy.size(); ++i) {
    new_indices[nodes_copy[i]] = i;
  }
}

void SceneGraph::update_node_local_transform(u32 node_id,
                                             const glm::mat4 &transform) {
  local_transforms[node_id] = transform;
  queue_to_update(node_id);
}

void SceneGraph::update_transforms(Renderer *renderer) {
  // NOTE: This assumes we have only 1 root node
  if (!nodes_to_update[0].empty()) {
    const u32 node_id = nodes_to_update[0][0];
    global_transforms[node_id] = local_transforms[node_id];
    nodes_to_update[0].clear();
  }

  for (u32 i = 1; i < MAX_NODE_LEVEL; ++i) {
    if (nodes_to_update[i].empty())
      continue;
    for (u32 c : nodes_to_update[i]) {
      u32 parent_id = nodes[c].parent_node;
      global_transforms[c] = global_transforms[parent_id] * local_transforms[c];
      u32 blas_instance_id = node_to_blas_instance[c];
      if (blas_instance_id != UINT32_MAX) {
        renderer->set_blas_instance_transform(blas_instance_id,
                                              global_transforms[c]);
      }
    }
    nodes_to_update[i].clear();
  }
}

void SceneGraph::delete_node(u32 node_id, Renderer *renderer) {
  // Do not delete the root
  if (node_id == 0)
    return;
  SceneNode &node = nodes[node_id];

  // NOTE: Should always have a parent node
  SceneNode &parent = nodes[node.parent_node];
  // Check if first child
  if (parent.first_child == node_id) {
    parent.first_child = node.next_sibling;
    // Case where node has siblings
    if (node.next_sibling != INVALID_NODE_ID) {
      nodes[node.next_sibling].last_sibling = node.last_sibling;
    }
  } else if (node.next_sibling == INVALID_NODE_ID) {
    // Last node
    // Get the second to last node
    for (u32 prev = parent.first_child;
         nodes[prev].next_sibling != INVALID_NODE_ID;
         prev = nodes[prev].next_sibling) {
      if (nodes[prev].next_sibling == node_id) {
        nodes[prev].next_sibling = INVALID_NODE_ID;
        nodes[parent.first_child].last_sibling = prev;
        break;
      }
    }
  } else {
    // Middle child
    for (u32 prev = parent.first_child;
         nodes[prev].next_sibling != INVALID_NODE_ID;
         prev = nodes[prev].next_sibling) {
      if (nodes[prev].next_sibling == node_id) {
        nodes[prev].next_sibling = node.next_sibling;
        break;
      }
    }
  }

  // Delete the blas instance if it has one
  u32 &blas_instance_id = node_to_blas_instance[node_id];
  if (blas_instance_id != UINT32_MAX) {
    renderer->remove_blas_instance(blas_instance_id);
    blas_instance_id = UINT32_MAX;
  }

  // Delete children aswell
  for (u32 c = node.first_child; c != INVALID_NODE_ID;
       c = nodes[c].next_sibling) {
    delete_node(c, renderer);
  }
  node_index_pool.release(node_id);
}

u32 render_scene_graph_nodes(const SceneGraph &scene_graph, u32 node_id,
                             u32 selected_node_id) {
  std::string_view node_name = scene_graph.get_node_name(node_id);
  const SceneNode &node = scene_graph.nodes[node_id];
  const bool is_leaf = node.first_child == INVALID_NODE_ID;
  ImGuiTreeNodeFlags flags =
      is_leaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet
              : ImGuiTreeNodeFlags_OpenOnDoubleClick;
  if (node_id == selected_node_id) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImVec4 color =
      is_leaf ? ImVec4(0.f, 1.f, 0.f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const bool is_opened = ImGui::TreeNodeEx(&scene_graph.nodes[node_id], flags,
                                           "%s", node_name.data());
  ImGui::PopStyleColor();
  ImGui::PushID(node_id);
  if (ImGui::IsItemClicked()) {
    selected_node_id = node_id;
  }

  // Recursively draw its children
  if (is_opened) {
    for (u32 child_id = node.first_child; child_id != INVALID_NODE_ID;
         child_id = scene_graph.nodes[child_id].next_sibling) {
      selected_node_id =
          render_scene_graph_nodes(scene_graph, child_id, selected_node_id);
    }
    ImGui::TreePop();
  }

  ImGui::PopID();

  return selected_node_id;
}

void render_scene_graph_nodes_property(SceneGraph &scene_graph, u32 node_id,
                                       Renderer *renderer) {
  if (node_id == INVALID_NODE_ID) {
    ImGui::Text("No node selected");
    return;
  }

  glm::vec3 local_translation = scene_graph.local_transforms[node_id][3];
  glm::vec3 global_translation = scene_graph.global_transforms[node_id][3];
  bool modified = false;
  ImGui::Text("Local Translation");
  modified |= ImGui::DragFloat3("Position", &local_translation.x, 0.5f, 0.f,
                                0.f, "%.3f");

  ImGui::Text("World Translation");
  ImGui::BeginDisabled();

  ImGui::PushItemWidth(ImGui::CalcItemWidth() / 3);
  ImGui::InputFloat("##Position_X", &global_translation.x);
  ImGui::SameLine();
  ImGui::InputFloat("##Position_Y", &global_translation.y);
  ImGui::SameLine();
  ImGui::InputFloat("##Position_Z", &global_translation.z);
  ImGui::SameLine();
  ImGui::Text("World Position");
  ImGui::PopItemWidth();

  ImGui::EndDisabled();

  const SceneNode &node = scene_graph.nodes[node_id];
  ImGui::Text("Parent Node: %s",
              node.parent_node == INVALID_NODE_ID
                  ? "INVALID_NODE_ID"
                  : std::to_string(node.parent_node).c_str());
  ImGui::Text("First Child: %s",
              node.first_child == INVALID_NODE_ID
                  ? "INVALID_NODE_ID"
                  : std::to_string(node.first_child).c_str());
  ImGui::Text("Next Sibling: %s",
              node.next_sibling == INVALID_NODE_ID
                  ? "INVALID_NODE_ID"
                  : std::to_string(node.next_sibling).c_str());
  ImGui::Text("Last Sibling: %s",
              node.last_sibling == INVALID_NODE_ID
                  ? "INVALID_NODE_ID"
                  : std::to_string(node.last_sibling).c_str());
  ImGui::Text("Level: %s", std::to_string(node.level).c_str());

  static bool add_node_clicked = false;
  if (ImGui::Button("Add Node")) {
    add_node_clicked = true;
  }

  if (add_node_clicked) {
    ImGui::SeparatorText("Add Node");
    static char node_name[50] = {0};
    ImGui::InputText("Node name", node_name, 50);

    const char *mesh_types[] = {"Plane", "Sphere", "Cube", "Gltf"};
    static int selected_mesh_type = 0;
    ImGui::Combo("Mesh Type", &selected_mesh_type, mesh_types,
                 IM_ARRAYSIZE(mesh_types));

    const char *material_types[] = {"Lambert", "Metal", "Dielectric",
                                    "Emissive", "None"};
    static int selected_material_type = MaterialType::NONE;
    ImGui::Combo("Material Type", &selected_material_type, material_types,
                 IM_ARRAYSIZE(material_types));

    static int selected_material_index_combo = 0;
    u32 material_index = UINT32_MAX;
    if (selected_material_type != MaterialType::NONE) {
      // TODO: Right now I am building the material_indices every frame instead
      // of caching it
      std::vector<int> material_indices;
      switch (selected_material_type) {
      case MaterialType::LAMBERT: {
        material_indices =
            std::vector<int>(renderer->lambert_mats.material_indices.begin(),
                             renderer->lambert_mats.material_indices.end());
        break;
      }
      case MaterialType::METAL: {
        material_indices =
            std::vector<int>(renderer->metal_mats.material_indices.begin(),
                             renderer->metal_mats.material_indices.end());
        break;
      }
      case MaterialType::DIELECTRIC: {
        material_indices =
            std::vector<int>(renderer->dielectric_mats.material_indices.begin(),
                             renderer->dielectric_mats.material_indices.end());
        break;
      }
      case MaterialType::EMISSIVE: {
        material_indices =
            std::vector<int>(renderer->emissive_mats.material_indices.begin(),
                             renderer->emissive_mats.material_indices.end());
        break;
      }
      default: {
        HERROR("render_scene_graph_nodes_property() - Unknown type");
        break;
      }
      }
      // TODO: Im also building this every frame
      auto getter = [](void *data, int idx, const char **out) -> bool {
        auto *indices = static_cast<std::vector<int> *>(data);
        static char label[32];
        snprintf(label, sizeof(label), "%d", (*indices)[idx]);
        *out = label;
        return true;
      };

      ImGui::Combo("Material Indices", &selected_material_index_combo, getter,
                   &material_indices, material_indices.size());
      material_index = material_indices[selected_material_index_combo];
    }

    if (ImGui::Button("Add")) {
      if (selected_mesh_type < 3) {
        u32 new_node_id;
        if (node_name[0] == 0) {
          new_node_id = scene_graph.add_node(node_id, std::string());
        } else {
          new_node_id = scene_graph.add_node(node_id, node_name);
          std::memset(node_name, 0, sizeof(char) * 50);
        }
        u32 blas_index;
        switch (selected_mesh_type) {
        case 0:
          blas_index = renderer->plane_blas_index;
          break;
        case 1:
          blas_index = renderer->sphere_blas_index;
          break;
        case 2:
          blas_index = renderer->cube_blas_index;
          break;
        default:
          blas_index = UINT32_MAX;
          HASSERT_MSG(false, "Unknown mesh type");
        }

        scene_graph.set_node_blas_instance(
            new_node_id, renderer->add_blas_instance(
                             blas_index, glm::mat4(1.f),
                             {.index = material_index,
                              .type = (MaterialType)selected_material_type}));
        scene_graph.update_node_local_transform(new_node_id, glm::mat4(1.f));
      } else if (selected_mesh_type == 3) {
        std::string file_path;
        std::string file_name;
        if (File::OpenFileDialog(file_name, file_path)) {
          if (file_path.size() && file_name.size()) {
            HASSERT(load_gltf_scene(scene_graph, renderer, file_path, file_name,
                                    node_id));
          }
        }
      }
      add_node_clicked = false;
      selected_mesh_type = 0;
      selected_material_type = 0;
      selected_material_index_combo = 0;
    }
    ImGui::SeparatorText("");
  }

  if (ImGui::Button("Delete Node")) {
    scene_graph.delete_node(node_id, renderer);
  }

  if (modified) {
    glm::mat4 new_transform = scene_graph.local_transforms[node_id];
    new_transform[3] = glm::vec4(local_translation, 1.f);

    scene_graph.update_node_local_transform(node_id, new_transform);
  }
}

void render_materials_window(Renderer *renderer,
                             MaterialHandle &selected_material) {
  ImGui::Begin("Materials");

  if (ImGui::BeginTabBar("MaterialTabs")) {
    // Lambert
    if (ImGui::BeginTabItem("Lambert")) {
      for (const u32 index : renderer->lambert_mats.material_indices) {
        char label[32];
        snprintf(label, sizeof(label), "Lambert %d", index);
        if (ImGui::Selectable(label, selected_material.index == index &&
                                         selected_material.type ==
                                             MaterialType::LAMBERT)) {
          selected_material.index = index;
          selected_material.type = MaterialType::LAMBERT;
        }
      }
      ImGui::EndTabItem();
      ImGui::SeparatorText("Selected Material");
      if (selected_material.index != UINT32_MAX &&
          selected_material.type == MaterialType::LAMBERT) {
        Lambert &mat =
            renderer->lambert_mats.materials[selected_material.index];
        ImGui::Text("Image View Index: %d", mat.index);
      }
      ImGui::SeparatorText("");
      const char *albedo_types[] = {"Vec3", "File"};
      static int selected_albedo_type = 0;
      ImGui::Combo("Albedo Type", &selected_albedo_type, albedo_types,
                   IM_ARRAYSIZE(albedo_types));
      static glm::vec3 col(0.f);

      if (selected_albedo_type == 0) {
        ImGui::InputFloat3("Albedo", &col.x);
      }
      if (ImGui::Button("Add Material")) {
        if (selected_albedo_type == 0) {
          renderer->add_lambert_material(col);
          col = glm::vec3(0.f);
        } else {
          std::string file_path;
          std::string file_name;
          if (File::OpenFileDialog(file_name, file_path)) {
            if (file_path.size() && file_name.size()) {
              renderer->add_lambert_material(file_path + "\\" + file_name);
            }
          }
        }
      }
    }

    // Metal
    if (ImGui::BeginTabItem("Metal")) {
      for (const u32 index : renderer->metal_mats.material_indices) {
        char label[32];
        snprintf(label, sizeof(label), "Metal %d", index);
        if (ImGui::Selectable(label, selected_material.index == index &&
                                         selected_material.type ==
                                             MaterialType::METAL)) {
          selected_material.index = index;
          selected_material.type = MaterialType::METAL;
        }
      }
      ImGui::EndTabItem();
      ImGui::SeparatorText("Selected Material");
      if (selected_material.index != UINT32_MAX &&
          selected_material.type == MaterialType::METAL) {
        Metal &mat = renderer->metal_mats.materials[selected_material.index];
        ImGui::Text("Albedo: %.3f, %.3f, %.3f", mat.albedo_fuzz[0],
                    mat.albedo_fuzz[1], mat.albedo_fuzz[2]);
        ImGui::Text("Fuzz: %.3f", mat.albedo_fuzz[3]);
      }
      ImGui::SeparatorText("");
      static glm::vec3 albedo(0.f);
      static f32 fuzz{0.f};
      ImGui::InputFloat3("Albedo", &albedo.x);
      ImGui::InputFloat("Fuzz", &fuzz);
      if (ImGui::Button("Add Material")) {
        renderer->add_metal_material(albedo, fuzz);
        albedo = glm::vec3(0.f);
        fuzz = 0.f;
      }
    }

    // Dielectric
    if (ImGui::BeginTabItem("Dielectric")) {
      for (const u32 index : renderer->dielectric_mats.material_indices) {
        char label[32];
        snprintf(label, sizeof(label), "Dielectric %d", index);
        if (ImGui::Selectable(label, selected_material.index == index &&
                                         selected_material.type ==
                                             MaterialType::DIELECTRIC)) {
          selected_material.index = index;
          selected_material.type = MaterialType::DIELECTRIC;
        }
      }
      ImGui::EndTabItem();
      ImGui::SeparatorText("Selected Material");
      if (selected_material.index != UINT32_MAX &&
          selected_material.type == MaterialType::DIELECTRIC) {
        Dielectric &mat =
            renderer->dielectric_mats.materials[selected_material.index];
        ImGui::Text("Refraction index: %.3f", mat.refraction_index);
      }
      ImGui::SeparatorText("");
      static f32 refraction_index{0.f};
      ImGui::InputFloat("Refraction Index", &refraction_index);
      if (ImGui::Button("Add Material")) {
        renderer->add_dielectric_material(refraction_index);
        refraction_index = 0.f;
      }
    }

    // Emissive
    if (ImGui::BeginTabItem("Emissive")) {
      for (const u32 index : renderer->emissive_mats.material_indices) {
        char label[32];
        snprintf(label, sizeof(label), "Emissive %d", index);
        if (ImGui::Selectable(label, selected_material.index == index &&
                                         selected_material.type ==
                                             MaterialType::EMISSIVE)) {
          selected_material.index = index;
          selected_material.type = MaterialType::EMISSIVE;
        }
      }
      ImGui::EndTabItem();
      ImGui::SeparatorText("Selected Material");
      if (selected_material.index != UINT32_MAX &&
          selected_material.type == MaterialType::EMISSIVE) {
        Emissive &mat =
            renderer->emissive_mats.materials[selected_material.index];
        ImGui::Text("Albedo: %.3f, %.3f, %.3f", mat.intensity[0],
                    mat.intensity[1], mat.intensity[2]);
      }
      ImGui::SeparatorText("");
      static glm::vec3 intensity(0.f);
      ImGui::InputFloat3("Intensity", &intensity.x);
      if (ImGui::Button("Add Material")) {
        renderer->add_emissive_material(intensity);
        intensity = glm::vec3(0.f);
      }
    }

    ImGui::EndTabBar();
  }

  if (selected_material.index != UINT32_MAX) {
    if (ImGui::Button("Delete Material")) {
      renderer->remove_material(selected_material);
      selected_material.index = UINT32_MAX;
    }
  }

  ImGui::End();
}

} // namespace hlx
