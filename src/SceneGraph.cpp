#include "SceneGraph.hpp"
#include "Core/Assert.hpp"
// Vendor
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>

namespace hlx {
i32 SceneGraph::add_node(u32 parent, i32 level, std::string name) {
  const u32 node_id = static_cast<u32>(nodes.size());
  local_transforms.push_back(glm::mat4(1.f));
  global_transforms.push_back(glm::mat4(1.f));
  const u32 string_id = static_cast<u32>(node_names.size());
  node_to_name[node_id] = string_id;
  // Check if a name was given
  if (name.empty()) {
    node_names.push_back(std::string("Node") + std::to_string(node_id));
  } else {
    node_names.push_back(name);
  }

  nodes.push_back({.parent_node = parent, .level = level});
  SceneNode &node = nodes[node_id];

  if (parent != invalid_node_id) {
    SceneNode &parent_node = nodes[parent];
    const i32 first_child = parent_node.first_child;

    if (first_child == invalid_node_id) {
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

  return node_id;
}

std::string_view SceneGraph::get_node_name(u32 node_id) const {
  auto it = node_to_name.find(node_id);
  HASSERT_MSGS(it != node_to_name.end(),
               "Node {} has no name in the node_to_name map", node_id);
  u32 name_id = it->first;
  return node_names[name_id];
}

u32 render_scene_graph(const SceneGraph &scene_graph, u32 node_id,
                       u32 selected_node_id) {
  std::string_view node_name = scene_graph.get_node_name(node_id);
  const SceneNode &node = scene_graph.nodes[node_id];
  const bool is_leaf = node.first_child == invalid_node_id;
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
    for (u32 child_id = node.first_child; child_id != invalid_node_id;
         child_id = scene_graph.nodes[child_id].next_sibling) {
      selected_node_id =
          render_scene_graph(scene_graph, child_id, selected_node_id);
    }
    ImGui::TreePop();
  }

  ImGui::PopID();

  return selected_node_id;
}

} // namespace hlx
