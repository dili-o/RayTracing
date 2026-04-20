#pragma once
#include "Core/FreeIndexPool.hpp"
#include "Material.hpp"
// Vendor
#include <glm/fwd.hpp>

constexpr u32 INVALID_NODE_ID = UINT32_MAX;
constexpr u32 MAX_NODE_LEVEL = 20;

namespace hlx {
struct Renderer;

struct SceneNode {
  u32 parent_node = INVALID_NODE_ID;
  u32 first_child = INVALID_NODE_ID;
  u32 next_sibling = INVALID_NODE_ID;
  // This is value is only set for the first_child of a node
  u32 last_sibling = INVALID_NODE_ID;
  i32 level = 0;
};

struct SceneGraph {
public:
  void init(u32 max_node_capacity = 10);
  void shutdown(Renderer *renderer);

  u32 add_node(u32 parent, std::string name);
  void set_node_blas_instance(u32 node_id, u32 blas_instance_id);
  std::string_view get_node_name(u32 node_id) const;
  void queue_to_update(u32 node_id);
  void update_node_local_transform(u32 node_id, const glm::mat4 &transform);
  void update_transforms(Renderer *renderer);
  void delete_node(u32 node_id, Renderer *renderer);

  void collect_nodes_to_delete(u32 node_id, std::vector<u32> &node_indices);
  void delete_scene_nodes(const std::vector<u32> &nodes_to_delete);

public:
  std::vector<SceneNode> nodes;
  std::vector<glm::mat4> local_transforms;
  std::vector<glm::mat4> global_transforms;
  // Key: node index, Value: blas_id
  std::unordered_map<u32, u32> node_to_blas_instance;
  std::vector<std::string> node_names;
  // List of nodes to update at each level
  std::vector<u32> nodes_to_update[MAX_NODE_LEVEL];
  // List of deleted nodes
  FreeIndexPool node_index_pool;
};

u32 render_scene_graph_nodes(const SceneGraph &scene_graph, u32 node_id,
                             u32 selected_node_id);
void render_scene_graph_nodes_property(SceneGraph &scene_graph, u32 node_id,
                                       Renderer *renderer);
void render_materials_window(Renderer *renderer,
                             MaterialHandle &selected_material);
} // namespace hlx
