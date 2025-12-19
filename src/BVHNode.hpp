#pragma once

#include "Triangle.hpp"

struct BVHNode {
  AABB bbox;
  u32 left_child;
  u32 first_prim, prim_count;
  bool is_leaf() const { return prim_count > 0; }
};

// TODO: Remove sphere support
class TriangleBVH : public Hittable {
public:
  TriangleBVH() {}

  bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
    return intersect_bvh(r, ray_t, rec, 0);
  }

  AABB bounding_box() const override {
    return bvh_nodes[0].bbox;
  }

  void add_triangle(std::shared_ptr<Triangle> trig) {
    triangles.push_back(trig);
  }

  void build_bvh() {
    HASSERT(bvh_nodes.size() == 0);
    const u32 N = triangles.size();
    bvh_nodes.resize(triangles.size() * 2 - 1);
    
    BVHNode& root = bvh_nodes[0];
    root.left_child = 0;
    root.first_prim = 0, root.prim_count = N;
    update_node_bounds(0);
    u32 nodes_used = 1;
    subdivide(0, nodes_used);
  }

private:
  void update_node_bounds(u32 node_idx) {
    BVHNode& node = bvh_nodes[node_idx];
    node.bbox = AABB::empty();

    for (u32 i = 0; i < node.prim_count; ++i) {
      auto &tri = triangles[node.first_prim + i];
      node.bbox.x.min = std::min(node.bbox.x.min, std::min(tri->v0.x, std::min(tri->v1.x, tri->v2.x)));
      node.bbox.x.max = std::max(node.bbox.x.max, std::max(tri->v0.x, std::max(tri->v1.x, tri->v2.x)));
      node.bbox.y.min = std::min(node.bbox.y.min, std::min(tri->v0.y, std::min(tri->v1.y, tri->v2.y)));
      node.bbox.y.max = std::max(node.bbox.y.max, std::max(tri->v0.y, std::max(tri->v1.y, tri->v2.y)));
      node.bbox.z.min = std::min(node.bbox.z.min, std::min(tri->v0.z, std::min(tri->v1.z, tri->v2.z)));
      node.bbox.z.max = std::max(node.bbox.z.max, std::max(tri->v0.z, std::max(tri->v1.z, tri->v2.z)));
    }
  }

  void subdivide(u32 node_idx, u32 &nodes_used) {
    BVHNode& node = bvh_nodes[node_idx];
    if (node.prim_count <= 2) return;
    // Determine split axis and position
    Vec3 aabb_min = Vec3(node.bbox.x.min, node.bbox.y.min, node.bbox.z.min);
    Vec3 aabb_max = Vec3(node.bbox.x.max, node.bbox.y.max, node.bbox.z.max);
    Vec3 extents = aabb_max - aabb_min;
    i32 axis = 0;
    if (extents.y > extents.x) axis = 1;
    if (extents.z > extents[axis]) axis = 2;
    f32 split_pos = aabb_min[axis] + extents[axis] * 0.5f;

    // Split triangles
    int i = node.first_prim;
		int j = i + node.prim_count - 1;
		while (i <= j) {
			if (triangles[i]->centroid[axis] < split_pos)
				i++;
			else
			std::swap(triangles[i], triangles[j--]);
		}

    // Create child nodes
    i32 left_count = i - node.first_prim;
    if (left_count == 0 || left_count == node.prim_count)
      return;
    i32 left_child_idx = nodes_used++;
    i32 right_child_idx = nodes_used++;
    node.left_child = left_child_idx;
    bvh_nodes[left_child_idx].first_prim = node.first_prim;
    bvh_nodes[left_child_idx].prim_count = left_count;
    bvh_nodes[right_child_idx].first_prim = i;
    bvh_nodes[right_child_idx].prim_count = node.prim_count - left_count;
    node.prim_count = 0;
    
    update_node_bounds(left_child_idx);
    update_node_bounds(right_child_idx);
    subdivide(left_child_idx, nodes_used);
    subdivide(right_child_idx, nodes_used);
  }

  bool intersect_bvh(const Ray& r, Interval ray_t, HitRecord& rec, u32 node_idx) const {
    const BVHNode& node = bvh_nodes[node_idx];
    if (!node.bbox.intersect(r, ray_t)) return false;
    bool hit = false;
    if (node.is_leaf()) {
      for (u32 i = 0; i < node.prim_count; ++i) {
        if (triangles[node.first_prim + i]->hit(r, ray_t, rec)) {
          hit = true;
        }
      }
    }
    else {
			hit |= intersect_bvh(r, ray_t, rec, node.left_child);
			hit |= intersect_bvh(r, ray_t, rec, node.left_child + 1);
    }
    return hit;
  }

private:
  std::vector<std::shared_ptr<Triangle>> triangles;
  std::vector<BVHNode> bvh_nodes;
};


