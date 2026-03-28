#pragma once

// Vendor
#include <glm/fwd.hpp>

constexpr u32 invalid_node_id = UINT32_MAX;

namespace hlx {
struct SceneNode {
  u32 parent_node = invalid_node_id;
  u32 first_child = invalid_node_id;
  u32 next_sibling = invalid_node_id;
  // This is value is only set for the first_child of a node
  u32 last_sibling = invalid_node_id;
  i32 level = 0;
};

struct SceneGraph {
public:
  i32 add_node(u32 parent, i32 level, std::string name);
  std::string_view get_node_name(u32 node_id) const;

public:
  std::vector<SceneNode> nodes;
  std::vector<glm::mat4> local_transforms;
  std::vector<glm::mat4> global_transforms;
  // Key: node index, Value: blas_id
  std::unordered_map<u32, u32> node_to_blas;
  // Key: node index, Value: name (string)
  std::unordered_map<u32, u32> node_to_name;
  std::vector<std::string> node_names;
};

u32 render_scene_graph(const SceneGraph &scene_graph, u32 node_id,
                       u32 selected_node_id);
} // namespace hlx
