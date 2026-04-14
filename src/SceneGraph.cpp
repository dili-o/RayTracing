#include "SceneGraph.hpp"
#include "Core/Assert.hpp"
#include "Material.hpp"
#include "Renderer.hpp"
// Vendor
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <imgui/imgui.h>
#include <numeric>

namespace hlx {

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

SceneGraph::SceneGraph(u32 max_node_capacity) {
  node_index_pool.init(max_node_capacity);
  nodes.resize(max_node_capacity);
  local_transforms.resize(max_node_capacity);
  global_transforms.resize(max_node_capacity);
  node_names.resize(max_node_capacity);
  add_node(INVALID_NODE_ID, "Root");
}

SceneGraph::~SceneGraph() {
  if (node_index_pool.size) {
    node_index_pool.release_all();
    node_index_pool.shutdown();
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
  u32 blas_instance_id = node_to_blas_instance[node_id];
  if (blas_instance_id != UINT32_MAX) {
    renderer->remove_blas_instance(blas_instance_id);
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

    const char *mesh_types[] = {"Plane", "Sphere", "Cube"};
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
            std::vector<int>(renderer->lambert_material_indices.begin(),
                             renderer->lambert_material_indices.end());
        break;
      }
      case MaterialType::METAL: {
        material_indices =
            std::vector<int>(renderer->metal_material_indices.begin(),
                             renderer->metal_material_indices.end());
        break;
      }
      case MaterialType::DIELECTRIC: {
        material_indices =
            std::vector<int>(renderer->dielectric_material_indices.begin(),
                             renderer->dielectric_material_indices.end());
        break;
      }
      case MaterialType::EMISSIVE: {
        material_indices =
            std::vector<int>(renderer->emissive_material_indices.begin(),
                             renderer->emissive_material_indices.end());
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
      for (const u32 index : renderer->lambert_material_indices) {
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
        Lambert &mat = renderer->lambert_materials[selected_material.index];
        ImGui::Text("Albedo: %.3f, %.3f, %.3f", mat.albedo[0], mat.albedo[1],
                    mat.albedo[2]);
      }
      ImGui::SeparatorText("");
      static glm::vec3 col(0.f);
      ImGui::InputFloat3("Albedo", &col.x);
      if (ImGui::Button("Add Material")) {
        renderer->add_lambert_material(col);
        col = glm::vec3(0.f);
      }
    }

    // Metal
    if (ImGui::BeginTabItem("Metal")) {
      for (const u32 index : renderer->metal_material_indices) {
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
        Metal &mat = renderer->metal_materials[selected_material.index];
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
      for (const u32 index : renderer->dielectric_material_indices) {
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
            renderer->dielectric_materials[selected_material.index];
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
      for (const u32 index : renderer->emissive_material_indices) {
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
        Emissive &mat = renderer->emissive_materials[selected_material.index];
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
